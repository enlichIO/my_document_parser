#include "pdf_parser.h"
#include "string_utils.h"
#include "memory.h"
#include "error.h"
#include "registry.h"
#include "registry.h"
#include "ocr_parser.h"
#include <mupdf/fitz.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <sys/stat.h>

// Prototipo esplicito per evitare errori se latex_converter.h non viene trovato
extern char* block_to_latex(const raw_text_block_t* block);

#ifdef DEBUG_PARSER
    #define DEBUG_LOG(fmt, ...) fprintf(stderr, "[PDF DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...)
#endif

// ============================================================================
// 1. BUFFER RIUTILIZZABILE
// ============================================================================
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} sbuf_t;

static bool sbuf_reserve(sbuf_t *b, size_t need) {
    if (need <= b->cap) return true;
    size_t cap = b->cap ? b->cap : 2048;
    while (cap < need) cap *= 2;
    char *p = realloc(b->data, cap);
    if (!p) return false;
    b->data = p;
    b->cap = cap;
    return true;
}

static void sbuf_free(sbuf_t *b) {
    if (b->data) free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static bool extract_block_into(fz_stext_block *block, sbuf_t *b) {
    b->len = 0;
    if (!block || block->type != FZ_STEXT_BLOCK_TEXT) return false;

    for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
        for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
            if (b->len + 4 >= b->cap) {
                if (!sbuf_reserve(b, b->len + 256)) return false;
            }
            int r = ch->c;
            char *p = b->data + b->len;
            if (r < 0x80) {
                *p++ = (char)r;
                b->len += 1;
            } else if (r < 0x800) {
                *p++ = 0xC0 | (r >> 6);
                *p++ = 0x80 | (r & 0x3F);
                b->len += 2;
            } else if (r < 0x10000) {
                *p++ = 0xE0 | (r >> 12);
                *p++ = 0x80 | ((r >> 6) & 0x3F);
                *p++ = 0x80 | (r & 0x3F);
                b->len += 3;
            } else {
                *p++ = 0xF0 | (r >> 18);
                *p++ = 0x80 | ((r >> 12) & 0x3F);
                *p++ = 0x80 | ((r >> 6) & 0x3F);
                *p++ = 0x80 | (r & 0x3F);
                b->len += 4;
            }
        }
        if (line->next) {
            if (b->len + 1 >= b->cap && !sbuf_reserve(b, b->len + 256)) return false;
            b->data[b->len++] = '\n';
        }
    }
    if (!sbuf_reserve(b, b->len + 1)) return false;
    b->data[b->len] = '\0';
    return b->len > 0;
}

// ============================================================================
// 2. DEDUPLICAZIONE CROSS-PAGE
// ============================================================================
#define DEDUP_SLOTS 8192

typedef struct {
    uint64_t key;
    uint32_t count;
    bool first_seen;
} dedup_slot_t;

static uint64_t boilerplate_hash(const char *s) {
    uint64_t h = 1469598103934665603ULL;
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c >= '0' && c <= '9') continue;
        if (c == ' ' || c == '\t' || c == '\n') continue;
        if (c >= 'A' && c <= 'Z') c += 32;
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h ? h : 1ULL;
}

static dedup_slot_t* dedup_lookup(dedup_slot_t *t, uint64_t k) {
    size_t i = (size_t)(k & (DEDUP_SLOTS - 1));
    while (t[i].key && t[i].key != k) i = (i + 1) & (DEDUP_SLOTS - 1);
    return &t[i];
}

static inline bool in_margin(const raw_text_block_t *b, float page_h) {
    return b->bbox_y1 < page_h * 0.14f || b->bbox_y0 > page_h * 0.88f;
}

static void strip_running_headers(parsed_document_t *doc, float page_h, uint32_t min_reps) {
    if (!doc || doc->page_count == 0) return;

    dedup_slot_t *table = calloc(DEDUP_SLOTS, sizeof(dedup_slot_t));
    if (!table) return;

    for (size_t p = 0; p < doc->page_count; p++) {
        parsed_page_t *pg = doc->pages[p];
        for (size_t b = 0; b < pg->block_count; b++) {
            raw_text_block_t *blk = pg->blocks[b];
            if (!blk || !blk->content || !in_margin(blk, page_h)) continue;
            if (strlen(blk->content) > 160) continue;
            
            uint64_t h = boilerplate_hash(blk->content);
            dedup_slot_t *s = dedup_lookup(table, h);
            if (!s->key) s->key = h;
            s->count++;
        }
    }

    size_t total_removed = 0;
    for (size_t p = 0; p < doc->page_count; p++) {
        parsed_page_t *pg = doc->pages[p];
        size_t w = 0;
        for (size_t b = 0; b < pg->block_count; b++) {
            raw_text_block_t *blk = pg->blocks[b];
            bool drop = false;
            
            if (blk && blk->content && in_margin(blk, page_h) && strlen(blk->content) <= 160) {
                uint64_t h = boilerplate_hash(blk->content);
                dedup_slot_t *s = dedup_lookup(table, h);
                
                if (s->key && s->count >= min_reps) {
                    if (!s->first_seen) {
                        s->first_seen = true;
                    } else {
                        drop = true;
                    }
                }
            }
            
            if (drop) {
                free_block(blk);
                total_removed++;
            } else {
                pg->blocks[w++] = blk;
            }
        }
        pg->block_count = w;
    }

    free(table);
    DEBUG_LOG("Rimossi %zu blocchi header/footer ripetuti", total_removed);
}

// ============================================================================
// 3. MERGE GEOMETRICO (Firme corrette con triplo puntatore)
// ============================================================================
typedef struct {
    char *content;
    fz_rect bbox;
    bool merged;
} raw_block_t;

typedef struct {
    raw_block_t **blocks;
    size_t count;
    fz_rect combined_bbox;
} block_group_t;

static void group_horizontal_blocks(raw_block_t *raw_blocks, size_t count,
                                     block_group_t ***groups, size_t *group_count) {
    *group_count = 0;
    *groups = calloc(count, sizeof(block_group_t*));
    if (!*groups) return;

    for (size_t i = 0; i < count; i++) {
        if (raw_blocks[i].merged) continue;

        block_group_t *group = malloc(sizeof(block_group_t));
        if (!group) continue;
        
        group->blocks = calloc(count, sizeof(raw_block_t*));
        if (!group->blocks) {
            free(group);
            continue;
        }
        group->count = 0;
        group->combined_bbox = raw_blocks[i].bbox;

        group->blocks[group->count++] = &raw_blocks[i];
        raw_blocks[i].merged = true;

        for (size_t j = i + 1; j < count; j++) {
            if (raw_blocks[j].merged) continue;

            float y_diff = fabs(raw_blocks[j].bbox.y0 - raw_blocks[i].bbox.y0);
            float x_gap = raw_blocks[j].bbox.x0 - group->combined_bbox.x1;

            if (y_diff < 5.0f && x_gap > 0 && x_gap < 30.0f) {
                group->blocks[group->count++] = &raw_blocks[j];
                raw_blocks[j].merged = true;
                if (raw_blocks[j].bbox.x1 > group->combined_bbox.x1) {
                    group->combined_bbox.x1 = raw_blocks[j].bbox.x1;
                }
            }
        }

        (*groups)[(*group_count)++] = group;
    }
}

static void group_vertical_blocks(block_group_t ***groups, size_t *group_count) {
    for (size_t i = 0; i < *group_count; i++) {
        block_group_t *g1 = (*groups)[i];
        if (!g1 || g1->count > 1) continue;

        raw_block_t *b1 = g1->blocks[0];
        if (strlen(b1->content) > 15) continue;

        size_t col_count = 1;
        for (size_t j = i + 1; j < *group_count && j < i + 10; j++) {
            block_group_t *gj = (*groups)[j];
            if (!gj || gj->count > 1) break;

            raw_block_t *bj = gj->blocks[0];
            if (strlen(bj->content) > 15) break;

            float x_diff = fabs(bj->bbox.x0 - b1->bbox.x0);
            float y_gap = bj->bbox.y0 - (*groups)[j-1]->combined_bbox.y1;

            if (x_diff < 10.0f && y_gap < 15.0f) {
                col_count++;
            } else {
                break;
            }
        }

        if (col_count >= 3) {
            block_group_t *merged = malloc(sizeof(block_group_t));
            if (!merged) continue;
            
            merged->blocks = calloc(col_count, sizeof(raw_block_t*));
            if (!merged->blocks) {
                free(merged);
                continue;
            }
            merged->count = col_count;
            merged->combined_bbox = g1->combined_bbox;

            for (size_t k = 0; k < col_count; k++) {
                merged->blocks[k] = (*groups)[i + k]->blocks[0];
                if ((*groups)[i + k]->combined_bbox.y1 > merged->combined_bbox.y1) {
                    merged->combined_bbox.y1 = (*groups)[i + k]->combined_bbox.y1;
                }
            }

            free(g1->blocks);
            free(g1);
            (*groups)[i] = merged;

            for (size_t k = 1; k < col_count; k++) {
                if ((*groups)[i + k]) {
                    free((*groups)[i + k]->blocks);
                    free((*groups)[i + k]);
                }
                (*groups)[i + k] = NULL;
            }

            size_t w = i + 1;
            for (size_t k = i + col_count; k < *group_count; k++) {
                (*groups)[w++] = (*groups)[k];
            }
            *group_count = w;
        }
    }
}

static char* merge_group_content(block_group_t *group) {
    if (!group || group->count == 0) return NULL;

    size_t total_len = 0;
    for (size_t i = 0; i < group->count; i++) {
        total_len += strlen(group->blocks[i]->content) + 1;
    }

    char *merged = malloc(total_len + 1);
    if (!merged) return NULL;
    merged[0] = '\0';

    for (size_t i = 0; i < group->count - 1; i++) {
        for (size_t j = i + 1; j < group->count; j++) {
            raw_block_t *bi = group->blocks[i];
            raw_block_t *bj = group->blocks[j];

            if (bj->bbox.y0 < bi->bbox.y0 - 2.0f) {
                group->blocks[i] = bj;
                group->blocks[j] = bi;
            } else if (fabs(bj->bbox.y0 - bi->bbox.y0) < 2.0f && bj->bbox.x0 < bi->bbox.x0) {
                group->blocks[i] = bj;
                group->blocks[j] = bi;
            }
        }
    }

    for (size_t i = 0; i < group->count; i++) {
        strcat(merged, group->blocks[i]->content);
        if (i < group->count - 1) {
            float y_diff = fabs(group->blocks[i+1]->bbox.y0 - group->blocks[i]->bbox.y0);
            strcat(merged, (y_diff > 5.0f) ? "\n" : " ");
        }
    }

    return merged;
}

// ============================================================================
// 4. CACHE SCALABILE
// ============================================================================
static size_t calculate_cache_size(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return 64 * 1024 * 1024;
    }

    size_t file_size = st.st_size;
    size_t cache_size = file_size * 2;

    if (cache_size < 32 * 1024 * 1024) cache_size = 32 * 1024 * 1024;
    if (cache_size > 256 * 1024 * 1024) cache_size = 256 * 1024 * 1024;

    DEBUG_LOG("File size: %zu bytes, Cache size: %zu bytes", file_size, cache_size);
    return cache_size;
}

// ============================================================================
// 5. MAIN PARSER
// ============================================================================
parsed_document_t* parse_pdf_file(const char* filepath, const char* password) {
    if (!filepath) {
        DEBUG_LOG("Failed: filepath is NULL");
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    DEBUG_LOG("Starting PDF parse of: %s", filepath);

    size_t cache_size = calculate_cache_size(filepath);
    fz_context *ctx = fz_new_context(NULL, NULL, cache_size);
    if (!ctx) {
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    fz_register_document_handlers(ctx);

    fz_document *mupdf_doc = NULL;
    parsed_document_t *result_doc = NULL;

    fz_try(ctx) {
        mupdf_doc = fz_open_document(ctx, filepath);
        if (password && fz_needs_password(ctx, mupdf_doc)) {
            if (!fz_authenticate_password(ctx, mupdf_doc, password)) {
                DEBUG_LOG("Incorrect password for PDF: %s", filepath);
                fz_drop_document(ctx, mupdf_doc);
                fz_drop_context(ctx);
                set_error(ERROR_INVALID_FORMAT);
                return NULL;
            }
        }
    } fz_catch(ctx) {
        DEBUG_LOG("Cannot open PDF file: %s", filepath);
        set_error(ERROR_FILE_NOT_FOUND);
        fz_drop_context(ctx);
        return NULL;
    }

    result_doc = create_document(filepath);
    if (!result_doc) {
        fz_drop_document(ctx, mupdf_doc);
        fz_drop_context(ctx);
        return NULL;
    }

    int page_count = fz_count_pages(ctx, mupdf_doc);
    
    char temp_content[512];
    snprintf(temp_content, sizeof(temp_content), "%s_pages_%d", filepath, page_count);
    result_doc->id = generate_document_id(filepath, temp_content, strlen(temp_content));

    if (registry_is_processed(result_doc->id)) {
        DEBUG_LOG("PDF already processed. ID: %s", result_doc->id);
        free_doc(result_doc);
        fz_drop_document(ctx, mupdf_doc);
        fz_drop_context(ctx);
        set_error(ERROR_ALL_OK);
        return NULL;
    }

    DEBUG_LOG("PDF has %d pages", page_count);

    sbuf_t scratch = {0};
    float page_h = 842.0f;

    for (int p = 0; p < page_count; p++) {
        fz_page *mupdf_page = NULL;
        fz_stext_page *text_page = NULL;
        parsed_page_t *our_page = NULL;

        fz_try(ctx) {
            mupdf_page = fz_load_page(ctx, mupdf_doc, p);
            
            if (p == 0) {
                fz_rect mb = fz_bound_page(ctx, mupdf_page);
                page_h = mb.y1 - mb.y0;
            }

            text_page = fz_new_stext_page_from_page(ctx, mupdf_page, NULL);

            raw_block_t *raw_blocks = NULL;
            size_t raw_count = 0;
            size_t raw_capacity = 0;

            for (fz_stext_block *block = text_page->first_block; block; block = block->next) {
                if (block->type == FZ_STEXT_BLOCK_TEXT) {
                    if (!extract_block_into(block, &scratch)) continue;

                    if (raw_count >= raw_capacity) {
                        raw_capacity = raw_capacity ? raw_capacity * 2 : 64;
                        raw_blocks = realloc(raw_blocks, raw_capacity * sizeof(raw_block_t));
                    }

                    raw_blocks[raw_count].content = strdup(scratch.data);
                    raw_blocks[raw_count].bbox = block->bbox;
                    raw_blocks[raw_count].merged = false;
                    raw_count++;
                } else if (block->type == FZ_STEXT_BLOCK_IMAGE) {
                    DEBUG_LOG("Image block on page %d - OCR not implemented in this version", p);
                }
            }

            block_group_t **groups = NULL;
            size_t group_count = 0;

            group_horizontal_blocks(raw_blocks, raw_count, &groups, &group_count);
            group_vertical_blocks(&groups, &group_count);

            our_page = create_page((size_t)p);
            if (!our_page) {
                fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create page structure");
            }

            for (size_t g = 0; g < group_count; g++) {
                if (!groups[g]) continue;

                char *merged_content = merge_group_content(groups[g]);
                if (!merged_content) continue;

                if (is_text_junk(merged_content)) {
                    DEBUG_LOG("Skipping junk block on page %d", p);
                    free(merged_content);
                    continue;
                }

                block_type_t type = determine_block_type(merged_content);

                if (type == BLOCK_TYPE_TEXT && is_likely_table(merged_content)) {
                    type = BLOCK_TYPE_TABLE;
                }

                char *final_content = merged_content;
                if (type != BLOCK_TYPE_TABLE) {
                    raw_text_block_t temp_block = {0};
                    temp_block.content = merged_content;
                    char *latex_content = block_to_latex(&temp_block);
                    if (latex_content) {
                        final_content = latex_content;
                        type = BLOCK_TYPE_MATH;
                        free(merged_content);
                    }
                }

                fz_rect bbox = groups[g]->combined_bbox;
                raw_text_block_t *final_block = create_block_pdf(
                    final_content, p, type, bbox.x0, bbox.y0, bbox.x1, bbox.y1
                );

                if (final_content != merged_content) free(final_content);
                else free(merged_content);

                if (final_block) {
                    if (add_block_to_page(our_page, final_block, result_doc->id) != 0) {
                        free_block(final_block);
                    }
                }
            }

            for (size_t i = 0; i < raw_count; i++) {
                free(raw_blocks[i].content);
            }
            free(raw_blocks);

            for (size_t i = 0; i < group_count; i++) {
                if (groups[i]) {
                    free(groups[i]->blocks);
                    free(groups[i]);
                }
            }
            free(groups);

            if (add_page_to_document(result_doc, our_page) != 0) {
                DEBUG_LOG("Failed to add page %d to document, skipping.", p);
            }

        } fz_catch(ctx) {
            DEBUG_LOG("MuPDF error on page %d: %s. Skipping page.", p, fz_caught_message(ctx));
            if (our_page) free_page(our_page);
        }

        if (text_page) fz_drop_stext_page(ctx, text_page);
        if (mupdf_page) fz_drop_page(ctx, mupdf_page);
    }

    strip_running_headers(result_doc, page_h, 3);

    registry_register(result_doc->id, filepath);

    sbuf_free(&scratch);
    fz_drop_document(ctx, mupdf_doc);
    fz_drop_context(ctx);

    DEBUG_LOG("Successfully parsed PDF with %zu pages", result_doc->page_count);
    set_error(ERROR_ALL_OK);
    return result_doc;
}