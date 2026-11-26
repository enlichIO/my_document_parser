#include "pdf_parser.h"
#include "string_utils.h"
#include "memory.h"
#include "error.h"
#include "registry.h"
#include <mupdf/fitz.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef DEBUG_PARSER
    #define DEBUG_LOG(fmt, ...) fprintf(stderr, "[PDF DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...)
#endif

static char* extract_block_text(fz_stext_block *block) {
    if (!block || block->type != FZ_STEXT_BLOCK_TEXT) return NULL;

    size_t total_length = 0;
    for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
        for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) total_length += 4;
        total_length += 1;
    }

    char *text = (char*)malloc(total_length + 1);
    if (!text) return NULL;

    char *ptr = text;
    for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
        for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
            int rune = ch->c;
            if (rune < 0x80) {
                *ptr++ = (char)rune;
            } else if (rune < 0x800) {
                *ptr++ = (char)(0xC0 | (rune >> 6)); *ptr++ = (char)(0x80 | (rune & 0x3F));
            } else if (rune < 0x10000) {
                *ptr++ = (char)(0xE0 | (rune >> 12)); *ptr++ = (char)(0x80 | ((rune >> 6) & 0x3F)); *ptr++ = (char)(0x80 | (rune & 0x3F));
            } else {
                *ptr++ = (char)(0xF0 | (rune >> 18)); *ptr++ = (char)(0x80 | ((rune >> 12) & 0x3F)); *ptr++ = (char)(0x80 | ((rune >> 6) & 0x3F)); *ptr++ = (char)(0x80 | (rune & 0x3F));
            }
        }
        if (line->next) *ptr++ = '\n';
    }
    *ptr = '\0';
    return text;
}

static bool process_text_block(fz_stext_block *block, int page_num, const char *doc_id, parsed_page_t *our_page) {
    if (!block || block->type != FZ_STEXT_BLOCK_TEXT) return false;

    char *block_text = extract_block_text(block);
    if (!block_text) return false;

    fz_rect bbox = block->bbox;
    char *normalized = normalize_text(block_text);
    free(block_text);

    if (!normalized || strlen(normalized) == 0) {
        free(normalized);
        return false;
    }

    block_type_t type = determine_block_type(normalized);
    raw_text_block_t *our_block = create_block_pdf(normalized, page_num, type, bbox.x0, bbox.y0, bbox.x1, bbox.y1);
    free(normalized);

    if (!our_block) return false;

    if (add_block_to_page(our_page, our_block, doc_id) != 0) {
        free_block(our_block);
        return false;
    }
    return true;
}

parsed_document_t* parse_pdf_file(const char* filepath, const char* password) {
    if (!filepath) {
        DEBUG_LOG("Failed: filepath is NULL");
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    DEBUG_LOG("Starting PDF parse of: %s", filepath);

    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
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
    
    // 1. Genera ID univoco (usiamo path + page_count per evitare di caricare GB di PDF in RAM per l'hash)
    char temp_content[512];
    snprintf(temp_content, sizeof(temp_content), "%s_pages_%d", filepath, page_count);
    result_doc->id = generate_document_id(filepath, temp_content, strlen(temp_content));

    // 2. Controlla registro
    if (registry_is_processed(result_doc->id)) {
        DEBUG_LOG("Documento PDF già processato. ID: %s", result_doc->id);
        free_doc(result_doc);
        fz_drop_document(ctx, mupdf_doc);
        fz_drop_context(ctx);
        set_error(ERROR_ALL_OK);
        return NULL;
    }

    DEBUG_LOG("PDF has %d pages", page_count);

    for (int p = 0; p < page_count; p++) {
        fz_page *mupdf_page = NULL;
        fz_stext_page *text_page = NULL;
        parsed_page_t *our_page = NULL;
        bool page_success = false;

        fz_try(ctx) {
            mupdf_page = fz_load_page(ctx, mupdf_doc, p);
            text_page = fz_new_stext_page_from_page(ctx, mupdf_page, NULL);

            our_page = create_page((size_t)p);
            if (!our_page) {
                DEBUG_LOG("Failed to create page %d structure, skipping page.", p);
            } else {
                for (fz_stext_block *block = text_page->first_block; block; block = block->next) {
                    if (block->type == FZ_STEXT_BLOCK_TEXT) {
                        process_text_block(block, p, result_doc->id, our_page);
                    } else if (block->type == FZ_STEXT_BLOCK_IMAGE) {
                        DEBUG_LOG("Skipping image block on page %d", p);
                    }
                }

                if (add_page_to_document(result_doc, our_page) == 0) {
                    page_success = true;
                } else {
                    DEBUG_LOG("Failed to add page %d to document, skipping.", p);
                }
            }
        } fz_catch(ctx) {
            DEBUG_LOG("MuPDF error on page %d: %s. Skipping page.", p, fz_caught_message(ctx));
        }

        if (!page_success && our_page) {
            free_page(our_page);
        }

        if (text_page) fz_drop_stext_page(ctx, text_page);
        if (mupdf_page) fz_drop_page(ctx, mupdf_page);
    }

    // 3. Deduplica i blocchi in ogni pagina
    for (size_t p = 0; p < result_doc->page_count; p++) {
        deduplicate_page_blocks(result_doc->pages[p], 0.90);
    }

    // 4. Registra il documento come processato
    registry_register(result_doc->id, filepath);

    fz_drop_document(ctx, mupdf_doc);
    fz_drop_context(ctx);

    DEBUG_LOG("Successfully parsed PDF with %zu pages", result_doc->page_count);
    set_error(ERROR_ALL_OK);
    return result_doc;
}