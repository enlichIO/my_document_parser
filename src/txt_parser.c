#include "txt_parser.h"
#include "string_utils.h"
#include "memory.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

// Adebugger-friendly logging macro.
#ifdef DEBUG_PARSER
    #define DEBUG_LOG(fmt, ...) fprintf(stderr, "[PARSER DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...) // Does nothing in production
#endif


parsed_document_t* parse_txt_file(const char* filepath) {
    if (!filepath) {
        DEBUG_LOG("Failed: filepath is NULL");
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    DEBUG_LOG("Starting parse of: %s", filepath);

    char* contents = read_file_contents(filepath);
    if (!contents) {
        DEBUG_LOG("Failed to read file contents. Error: %s", parser_error_to_string(get_error()));
        return NULL; // Error already set by read_file_contents
    }

    bool use_double_lines = double_newlines_parag(contents);
    DEBUG_LOG("Heuristic chose double newlines: %s", use_double_lines ? "YES" : "NO");

    size_t paragraph_count = 0;
    char** paragraphs = split_into_paragraphs(contents, &paragraph_count, use_double_lines);
    if (!paragraphs) {
        DEBUG_LOG("Failed to split paragraphs.");
        cleanup_parse_state(NULL, NULL, NULL, 0, contents);
        return NULL;
    }

    parsed_document_t* document = create_document(filepath);
    if (!document) {
        DEBUG_LOG("Failed to create document structure.");
        cleanup_parse_state(NULL, NULL, paragraphs, paragraph_count, contents);
        return NULL;
    }

    document->id = strdup("txt_doc"); 
    if (!document->id) {
        DEBUG_LOG("Failed to allocate document ID.");
        cleanup_parse_state(document, NULL, paragraphs, paragraph_count, contents);
        return NULL;
    }

    parsed_page_t* page = create_page(0);
    if (!page) {
        DEBUG_LOG("Failed to create page structure.");
        cleanup_parse_state(document, NULL, paragraphs, paragraph_count, contents);
        return NULL;
    }

    for (size_t i = 0; i < paragraph_count; i++) {
        char* normalized_paragraph = normalize_text(paragraphs[i]);
        if (!normalized_paragraph) {
            DEBUG_LOG("Failed to normalize paragraph %zu.", i);
            cleanup_parse_state(document, page, paragraphs, paragraph_count, contents);
            return NULL;
        }

        block_type_t type = determine_block_type(normalized_paragraph);
        raw_text_block_t* block = create_block(normalized_paragraph, 0, type, 0, 0);
        free(normalized_paragraph); 

        if (!block) {
            DEBUG_LOG("Failed to create block for paragraph %zu.", i);
            cleanup_parse_state(document, page, paragraphs, paragraph_count, contents);
            return NULL;
        }

        if (add_block_to_page(page, block, document->id) != 0) {
            DEBUG_LOG("Failed to add block %zu to page.", i);
            if (block->id) free(block->id);
            if (block->content) free(block->content);
            free(block);
            
            cleanup_parse_state(document, page, paragraphs, paragraph_count, contents);
            return NULL;
        }
    }

    if (add_page_to_document(document, page) != 0) {
        DEBUG_LOG("Failed to add page to document.");
        cleanup_parse_state(document, page, paragraphs, paragraph_count, contents);
        return NULL;
    }

    // SUCCESS: Clean up temporary artifacts, but NOT the document or page
    cleanup_parse_state(NULL, NULL, paragraphs, paragraph_count, contents);
    
    DEBUG_LOG("Successfully parsed document with %zu pages.", document->page_count);
    set_error(ERROR_ALL_OK);
    return document;
}