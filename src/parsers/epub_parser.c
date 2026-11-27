#include "epub_parser.h"
#include "string_utils.h"
#include "memory.h"
#include "error.h"
#include "registry.h"
#include "latex_converter.h" // ✅ AGGIUNTO: Conversione LaTeX
#include <zip.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h> // ✅ AGGIUNTO: bool, true, false
#include <math.h>    // ✅ AGGIUNTO: fabs

#ifdef DEBUG_PARSER
    #define DEBUG_LOG(fmt, ...) fprintf(stderr, "[EPUB DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...)
#endif

// ============================================================================
// BUFFER RIUTILIZZABILE (elimina allocazioni durante estrazione HTML)
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

// Estrae testo da HTML nel buffer riutilizzabile (zero malloc intermedi!)
static bool extract_text_from_html_into(const char *html, sbuf_t *b) {
    b->len = 0;
    if (!html) return false;

    size_t html_len = strlen(html);
    if (!sbuf_reserve(b, html_len + 1)) return false;

    bool in_tag = false;
    for (size_t i = 0; i < html_len; i++) {
        if (html[i] == '<') {
            in_tag = true;
            if (b->len > 0 && b->data[b->len - 1] != ' ' && b->data[b->len - 1] != '\n') {
                if (!sbuf_reserve(b, b->len + 2)) return false;
                b->data[b->len++] = ' ';
            }
        } else if (html[i] == '>') {
            in_tag = false;
        } else if (!in_tag) {
            if (!sbuf_reserve(b, b->len + 2)) return false;
            b->data[b->len++] = html[i];
        }
    }

    if (!sbuf_reserve(b, b->len + 1)) return false;
    b->data[b->len] = '\0';
    return b->len > 0;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
static char* extract_attr_value(const char *xml, const char *attr_name) {
    char search[64];
    snprintf(search, sizeof(search), "%s=\"", attr_name);

    const char *start = strstr(xml, search);
    if (!start) return NULL;

    start += strlen(search);
    const char *end = strchr(start, '"');
    if (!end) return NULL;

    size_t len = end - start;
    char *value = malloc(len + 1);
    if (!value) return NULL;

    memcpy(value, start, len);
    value[len] = '\0';
    return value;
}

static char* read_zip_file(zip_t *archive, const char *filename) {
    struct zip_stat st;
    if (zip_stat(archive, filename, 0, &st) != 0) return NULL;

    zip_file_t *file = zip_fopen(archive, filename, 0);
    if (!file) return NULL;

    char *buffer = malloc(st.size + 1);
    if (!buffer) {
        zip_fclose(file);
        return NULL;
    }

    zip_int64_t bytes = zip_fread(file, buffer, st.size);
    zip_fclose(file);

    if (bytes != (zip_int64_t)st.size) {
        free(buffer);
        return NULL;
    }

    buffer[st.size] = '\0';
    return buffer;
}

// ============================================================================
// DEDUPLICAZIONE CROSS-CHAPTER (rimuove header/footer ripetuti tra capitoli)
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

// Negli EPUB, header/footer sono tipicamente i primi/ultimi blocchi di ogni capitolo
// e sono corti (< 100 chars). Questa funzione li rimuove se ripetuti >= min_reps volte.
static void strip_epub_running_headers(parsed_document_t *doc, uint32_t min_reps) {
    if (!doc || doc->page_count == 0) return;

    dedup_slot_t *table = calloc(DEDUP_SLOTS, sizeof(dedup_slot_t));
    if (!table) return;

    // Pass 1: Conta blocchi candidati (primi/ultimi di ogni capitolo, corti)
    for (size_t p = 0; p < doc->page_count; p++) {
        parsed_page_t *pg = doc->pages[p];
        if (pg->block_count == 0) continue;

        // Controlla primo blocco
        raw_text_block_t *first = pg->blocks[0];
        if (first && first->content && strlen(first->content) <= 100) {
            uint64_t h = boilerplate_hash(first->content);
            dedup_slot_t *s = dedup_lookup(table, h);
            if (!s->key) s->key = h;
            s->count++;
        }

        // Controlla ultimo blocco
        if (pg->block_count > 1) {
            raw_text_block_t *last = pg->blocks[pg->block_count - 1];
            if (last && last->content && strlen(last->content) <= 100) {
                uint64_t h = boilerplate_hash(last->content);
                dedup_slot_t *s = dedup_lookup(table, h);
                if (!s->key) s->key = h;
                s->count++;
            }
        }
    }

    // Pass 2: Rimuovi ripetizioni, mantieni prima occorrenza
    size_t total_removed = 0;
    for (size_t p = 0; p < doc->page_count; p++) {
        parsed_page_t *pg = doc->pages[p];
        if (pg->block_count == 0) continue;

        // Controlla primo blocco
        raw_text_block_t *first = pg->blocks[0];
        bool drop_first = false;
        if (first && first->content && strlen(first->content) <= 100) {
            uint64_t h = boilerplate_hash(first->content);
            dedup_slot_t *s = dedup_lookup(table, h);
            if (s->key && s->count >= min_reps) {
                if (!s->first_seen) {
                    s->first_seen = true;
                } else {
                    drop_first = true;
                }
            }
        }

        // Controlla ultimo blocco
        bool drop_last = false;
        if (pg->block_count > 1) {
            raw_text_block_t *last = pg->blocks[pg->block_count - 1];
            if (last && last->content && strlen(last->content) <= 100) {
                uint64_t h = boilerplate_hash(last->content);
                dedup_slot_t *s = dedup_lookup(table, h);
                if (s->key && s->count >= min_reps) {
                    if (!s->first_seen) {
                        s->first_seen = true;
                    } else {
                        drop_last = true;
                    }
                }
            }
        }

        // Applica rimozioni
        size_t w = 0;
        for (size_t b = 0; b < pg->block_count; b++) {
            bool drop = false;
            if (b == 0 && drop_first) drop = true;
            if (b == pg->block_count - 1 && drop_last && pg->block_count > 1) drop = true;

            if (drop) {
                free_block(pg->blocks[b]);
                total_removed++;
            } else {
                pg->blocks[w++] = pg->blocks[b];
            }
        }
        pg->block_count = w;
    }

    free(table);
    DEBUG_LOG("Rimossi %zu blocchi header/footer EPUB ripetuti", total_removed);
}

// ============================================================================
// MAIN PARSER
// ============================================================================
parsed_document_t* parse_epub_file(const char* filepath) {
    if (!filepath) {
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    DEBUG_LOG("Starting EPUB parse of: %s", filepath);

    int err;
    zip_t *archive = zip_open(filepath, 0, &err);
    if (!archive) {
        set_error(ERROR_FILE_NOT_FOUND);
        return NULL;
    }

    // 1. Leggi container.xml per trovare content.opf
    char *container = read_zip_file(archive, "META-INF/container.xml");
    if (!container) {
        zip_close(archive);
        return NULL;
    }

    char *opf_path = extract_attr_value(container, "full-path");
    free(container);

    if (!opf_path) {
        zip_close(archive);
        return NULL;
    }

    // 2. Leggi content.opf
    char *opf = read_zip_file(archive, opf_path);
    if (!opf) {
        free(opf_path);
        zip_close(archive);
        return NULL;
    }

    // 3. Crea documento
    parsed_document_t *doc = create_document(filepath);
    if (!doc) {
        free(opf);
        free(opf_path);
        zip_close(archive);
        return NULL;
    }

    // 4. Genera ID univoco basato sul contenuto di opf
    doc->id = generate_document_id(filepath, opf, strlen(opf));

    // 5. Controlla registro
    if (registry_is_processed(doc->id)) {
        DEBUG_LOG("EPUB già processato in precedenza. ID: %s", doc->id);
        free_doc(doc);
        free(opf);
        free(opf_path);
        zip_close(archive);
        set_error(ERROR_ALL_OK);
        return NULL;
    }

    // 6. Buffer riutilizzabile per estrazione HTML
    sbuf_t scratch = {0};

    // 7. Estrai idref dallo spine e cerca href nel manifest
    int page_num = 0;
    const char *p = opf;

    while ((p = strstr(p, "<itemref")) != NULL) {
        char *idref = extract_attr_value(p, "idref");
        if (!idref) break;

        char search[256];
        snprintf(search, sizeof(search), "id=\"%s\"", idref);
        char *manifest_item = strstr(opf, search);

        if (manifest_item) {
            char *href = extract_attr_value(manifest_item, "href");

            if (href) {
                // Costruisci path completo
                char full_path[512];
                char *last_slash = strrchr(opf_path, '/');
                if (last_slash) {
                    size_t base_len = last_slash - opf_path + 1;
                    snprintf(full_path, sizeof(full_path), "%.*s%s", (int)base_len, opf_path, href);
                } else {
                    snprintf(full_path, sizeof(full_path), "%s", href);
                }

                // Leggi file XHTML
                char *xhtml = read_zip_file(archive, full_path);

                if (xhtml) {
                    // ✅ Estrai testo nel buffer riutilizzabile (zero malloc!)
                    if (extract_text_from_html_into(xhtml, &scratch)) {
                        char *normalized = normalize_text(scratch.data);

                        if (normalized && strlen(normalized) > 0) {
                            block_type_t type = determine_block_type(normalized);

                            // ✅ Applica conversione LaTeX se contiene simboli matematici
                            raw_text_block_t temp_block = {0};
                            temp_block.content = normalized;
                            char *latex_content = block_to_latex(&temp_block);

                            if (latex_content) {
                                free(normalized);
                                normalized = latex_content;
                                type = BLOCK_TYPE_MATH;
                            }

                            parsed_page_t *page = create_page((size_t)page_num);

                            if (page) {
                                raw_text_block_t *block = create_block_pdf(
                                    normalized, page_num, type, 0.0f, 0.0f, 0.0f, 0.0f
                                );
                                free(normalized);

                                if (block) {
                                    if (add_block_to_page(page, block, doc->id) == 0) {
                                        if (add_page_to_document(doc, page) != 0) {
                                            free_page(page);
                                        } else {
                                            page_num++;
                                        }
                                    } else {
                                        free_block(block);
                                        free_page(page);
                                    }
                                } else {
                                    free_page(page);
                                }
                            } else {
                                free(normalized);
                            }
                        } else {
                            free(normalized);
                        }
                    }
                    free(xhtml);
                }
                free(href);
            }
        }

        free(idref);
        p += 8; // Salta "<itemref"
    }

    // 8. Cleanup risorse temporanee
    sbuf_free(&scratch);
    free(opf);
    free(opf_path);
    zip_close(archive);

    // 9. ✅ Deduplicazione cross-chapter (rimuove header/footer ripetuti tra capitoli)
    strip_epub_running_headers(doc, 3);

    // 10. Registra documento
    registry_register(doc->id, filepath);

    DEBUG_LOG("Successfully parsed EPUB with %d pages", page_num);
    set_error(ERROR_ALL_OK);
    return doc;
}