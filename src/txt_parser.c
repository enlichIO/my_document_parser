#include "txt_parser.h"
#include "string_utils.h"
#include "memory.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef DEBUG_PARSER
    #define DEBUG_LOG(fmt, ...) fprintf(stderr, "[PARSER DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...) // Does nothing in production
#endif



// Calculate the starting line of each paragraph and the final line position.
static size_t* calculate_line_numbers(const char* text, char** paragraphs, size_t paragraph_count) {
    size_t* line_numbers = (size_t*)malloc((paragraph_count + 1) * sizeof(size_t));
    if (!line_numbers) return NULL;

    size_t current_line = 1;
    const char* ptr = text;

    for (size_t i = 0; i < paragraph_count; i++) {
        line_numbers[i] = current_line;

        const char* para_start = strstr(ptr, paragraphs[i]);
        if (para_start) {
            const char* para_end = para_start + strlen(paragraphs[i]);
            for (const char* p = para_start; p < para_end; p++) {
                if (*p == '\n') {
                    current_line++;
                }
            }
            ptr = para_end;
        }
    }

    line_numbers[paragraph_count] = current_line;
    return line_numbers;
}

// ============================================================================
// MAIN PARSER FUNCTION
// ============================================================================
parsed_document_t* parse_txt_file(const char* filepath) {
    if (!filepath) {
        DEBUG_LOG("Failed: filepath is NULL");
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    DEBUG_LOG("Starting parse of: %s", filepath);

    // 1. Read file
    char* contents = read_file_contents(filepath);
    if (!contents) {
        DEBUG_LOG("Failed to read file contents. Error: %s", parser_error_to_string(get_error()));
        return NULL;
    }

    // 2. Determine split strategy
    bool use_double_lines = double_newlines_parag(contents);
    DEBUG_LOG("Heuristic chose double newlines: %s", use_double_lines ? "YES" : "NO");

    // 3. Split into paragraphs
    size_t paragraph_count = 0;
    char** paragraphs = split_into_paragraphs(contents, &paragraph_count, use_double_lines);
    if (!paragraphs) {
        DEBUG_LOG("Failed to split paragraphs.");
        // 5 arguments: doc, page, string_array, string_count, single_string
        cleanup_parse_state(NULL, NULL, NULL, 0, contents);
        return NULL;
    }

    // 4. Calculate source line numbers
    size_t* line_numbers = calculate_line_numbers(contents, paragraphs, paragraph_count);
    if (!line_numbers) {
        DEBUG_LOG("Failed to calculate line numbers.");
        cleanup_parse_state(NULL, NULL, paragraphs, paragraph_count, contents);
        return NULL;
    }

    // 5. Create document structure
    parsed_document_t* document = create_document(filepath);
    if (!document) {
        DEBUG_LOG("Failed to create document structure.");
        free(line_numbers);
        cleanup_parse_state(NULL, NULL, paragraphs, paragraph_count, contents);
        return NULL;
    }

    document->id = strdup("txt_doc");
    if (!document->id) {
        DEBUG_LOG("Failed to allocate document ID.");
        free(line_numbers);
        cleanup_parse_state(document, NULL, paragraphs, paragraph_count, contents);
        return NULL;
    }

    // 6. Create single page (TXT files are single-page)
    parsed_page_t* page = create_page(0);
    if (!page) {
        DEBUG_LOG("Failed to create page structure.");
        free(line_numbers);
        cleanup_parse_state(document, NULL, paragraphs, paragraph_count, contents);
        return NULL;
    }

    // 7. Process each paragraph
    for (size_t i = 0; i < paragraph_count; i++) {
        char* normalized_paragraph = normalize_text(paragraphs[i]);
        if (!normalized_paragraph) {
            DEBUG_LOG("Failed to normalize paragraph %zu.", i);
            free(line_numbers);
            cleanup_parse_state(document, page, paragraphs, paragraph_count, contents);
            return NULL;
        }

        block_type_t type = determine_block_type(normalized_paragraph);
        
        size_t line_start = line_numbers[i];
        size_t line_end = (i + 1 < paragraph_count) ? line_numbers[i + 1] - 1 : line_numbers[paragraph_count];
        
        // ✅ USE THE TXT WRAPPER (4 arguments)
        raw_text_block_t* block = create_block_txt(normalized_paragraph, line_start, line_end, type);
        free(normalized_paragraph);

        if (!block) {
            DEBUG_LOG("Failed to create block for paragraph %zu.", i);
            free(line_numbers);
            cleanup_parse_state(document, page, paragraphs, paragraph_count, contents);
            return NULL;
        }

        if (add_block_to_page(page, block, document->id) != 0) {
            DEBUG_LOG("Failed to add block %zu to page.", i);
            free_block(block);
            free(line_numbers);
            cleanup_parse_state(document, page, paragraphs, paragraph_count, contents);
            return NULL;
        }
    }

    // 8. Add page to document
    if (add_page_to_document(document, page) != 0) {
        DEBUG_LOG("Failed to add page to document.");
        free(line_numbers);
        cleanup_parse_state(document, page, paragraphs, paragraph_count, contents);
        return NULL;
    }

    // 9. SUCCESS: Free line_numbers, then use generic cleanup for temporary artifacts
    free(line_numbers);
    cleanup_parse_state(NULL, NULL, paragraphs, paragraph_count, contents);

    DEBUG_LOG("Successfully parsed document with %zu pages.", document->page_count);
    set_error(ERROR_ALL_OK);
    return document;
}