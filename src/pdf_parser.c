#include "pdf_parser.h"
#include "string_utils.h"
#include "memory.h"
#include "error.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <mupdf/fitz.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef DEBUG_PARSER
    #define DEBUG_LOG(fmt, ...) fprintf(stderr, "[PDF DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...)
#endif

// converter from pdf_binary to standard text
static char* converter_(fz_stext_block *block) {
    if (!block || block->type != FZ_STEXT_BLOCK_TEXT) {
        return NULL;
    }

    // calculate total length needed 
    size_t total_length = 0;
    for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
        for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
            total_length += 4; 
        }
        total_length += 1; // newline between lines
    }

    char *text = (char*)malloc(total_length + 1);
    if (!text) return NULL;

    //  convert Unicode code points to UTF-8
    char *ptr = text;
    for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
        for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
            int rune = ch->c;
            
            if (rune < 0x80) {
                *ptr++ = (char)rune;
            } else if (rune < 0x800) {
                *ptr++ = (char)(0xC0 | (rune >> 6));
                *ptr++ = (char)(0x80 | (rune & 0x3F));
            } else if (rune < 0x10000) {
                *ptr++ = (char)(0xE0 | (rune >> 12));
                *ptr++ = (char)(0x80 | ((rune >> 6) & 0x3F));
                *ptr++ = (char)(0x80 | (rune & 0x3F));
            } else {
                *ptr++ = (char)(0xF0 | (rune >> 18));
                *ptr++ = (char)(0x80 | ((rune >> 12) & 0x3F));
                *ptr++ = (char)(0x80 | ((rune >> 6) & 0x3F));
                *ptr++ = (char)(0x80 | (rune & 0x3F));
            }
        }
        if (line->next) {
            *ptr++ = '\n';
        }
    }
    
    *ptr = '\0';
    return text;
}


static bool process_text_block(fz_stext_block *block, int page_num, const char *doc_id, parsed_page_t *our_page) {
    if (!block || block->type != FZ_STEXT_BLOCK_TEXT) return false;

    // 1. Extract raw text
    char *block_text = converter_(block);
    if (!block_text) return false;

    //  Get bounding box BEFORE freeing anything
    fz_rect bbox = block->bbox;

    //  Normalize
    char *normalized = normalize_text(block_text);
    free(block_text); // Done with raw extraction

    if (!normalized || strlen(normalized) == 0) {
        free(normalized);
        return false; // Skip completely blank blocks
    }

    block_type_t type = determine_block_type(normalized);

    // 4. Create and add block
    raw_text_block_t *our_block = create_block_pdf(
        normalized, page_num, type, bbox.x0, bbox.y0, bbox.x1, bbox.y1
    );
    free(normalized); // create_block_pdf duplicated the string

    if (!our_block) {
        return false; // Skip if memory allocation failed
    }

    if (add_block_to_page(our_page, our_block, doc_id) != 0) {
        free_block(our_block); // Clean up the rejected block
        return false; // Skip if adding failed
    }

    return true; // Success
}



// to do pdf parser
parsed_document_t* parse_pdf_file(const char* filepath, const char* password) {
    if (!filepath) {
        DEBUG_LOG("Failed: filepath is NULL");
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    DEBUG_LOG("Starting PDF parse of: %s", filepath);

    // MuPDF context
    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!ctx) {
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    fz_register_document_handlers(ctx);

    fz_document *mupdf_doc = NULL;
    parsed_document_t *result_doc = NULL;

    // Open document (with password)
    fz_try(ctx) {
        mupdf_doc = fz_open_document(ctx, filepath);
        
        // If a password is provided and the doc needs it, authenticate
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

    //  Create document structure
    result_doc = create_document(filepath);
    if (!result_doc) {
        fz_drop_document(ctx, mupdf_doc);
        fz_drop_context(ctx);
        return NULL; // Error already set by create_document
    }

    result_doc->id = strdup("pdf_doc");
    if (!result_doc->id) {
        cleanup_parse_state(result_doc, NULL, NULL, 0, NULL);
        fz_drop_document(ctx, mupdf_doc);
        fz_drop_context(ctx);
        return NULL;
    }

    int page_count = fz_count_pages(ctx, mupdf_doc);
    DEBUG_LOG("PDF has %d pages", page_count);

    // Iterate through each page ( dont like this metod but tutorial said so)
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
                // ✅ FIXED: Modern MuPDF uses a linked list, not an array!
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

        // Cleanup orphaned page if it wasn't successfully added to the document
        if (!page_success && our_page) {
            free_page(our_page);
        }

        // Cleanup MuPDF resources for this specific page (safe even if NULL)
        if (text_page) fz_drop_stext_page(ctx, text_page);
        if (mupdf_page) fz_drop_page(ctx, mupdf_page);
    }

    // 5. Final MuPDF Cleanup (success path)
    fz_drop_document(ctx, mupdf_doc);
    fz_drop_context(ctx);

    DEBUG_LOG("Successfully parsed PDF with %zu pages", result_doc->page_count);
    set_error(ERROR_ALL_OK);
    return result_doc;
}