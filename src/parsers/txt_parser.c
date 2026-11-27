#include "txt_parser.h"
#include "string_utils.h"
#include "memory.h"
#include "error.h"
#include "registry.h"
#include "latex_converter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef DEBUG_PARSER
    #define DEBUG_LOG(fmt, ...) fprintf(stderr, "[TXT DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...)
#endif



// HELPER: Calcolo numeri di riga
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
                if (*p == '\n') current_line++;
            }
            ptr = para_end;
        }
    }
    line_numbers[paragraph_count] = current_line;
    return line_numbers;
}

// MAIN PARSER
parsed_document_t* parse_txt_file(const char* filepath) {
    if (!filepath) {
        DEBUG_LOG("Failed: filepath is NULL");
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    DEBUG_LOG("Starting TXT parse of: %s", filepath);

    //Leggi contenuto file
    char* contents = read_file_contents(filepath);
    if (!contents) {
        DEBUG_LOG("Failed to read file contents. Error: %s", parser_error_to_string(get_error()));
        return NULL;
    }

    //Genera ID univoco basato sul contenuto
    char* doc_id = generate_document_id(filepath, contents, strlen(contents));
    if (!doc_id) {
        free(contents);
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    // 3. Controlla registro
    if (registry_is_processed(doc_id)) {
        DEBUG_LOG("TXT already processed. ID: %s", doc_id);
        free(contents);
        free(doc_id);
        set_error(ERROR_ALL_OK);
        return NULL;
    }

    //Split in paragrafi
    bool use_double_lines = double_newlines_parag(contents);
    DEBUG_LOG("Heuristic chose double newlines: %s", use_double_lines ? "YES" : "NO");

    size_t paragraph_count = 0;
    char** paragraphs = split_into_paragraphs(contents, &paragraph_count, use_double_lines);
    if (!paragraphs) {
        DEBUG_LOG("Failed to split paragraphs.");
        free(contents);
        free(doc_id);
        return NULL;
    }

    // 5. Calcola numeri di riga
    size_t* line_numbers = calculate_line_numbers(contents, paragraphs, paragraph_count);
    if (!line_numbers) {
        DEBUG_LOG("Failed to calculate line numbers.");
        cleanup_parse_state(NULL, NULL, paragraphs, paragraph_count, contents);
        free(doc_id);
        return NULL;
    }

    //Crea struttura documento
    parsed_document_t* document = create_document(filepath);
    if (!document) {
        DEBUG_LOG("Failed to create document structure.");
        free(line_numbers);
        cleanup_parse_state(NULL, NULL, paragraphs, paragraph_count, contents);
        free(doc_id);
        return NULL;
    }

    document->id = doc_id; // Trasferisce ownership dell'ID

    //Crea pagina e blocchi con buffer riutilizzabile
    parsed_page_t* page = create_page(0);
    if (!page) {
        DEBUG_LOG("Failed to create page structure.");
        free(line_numbers);
        cleanup_parse_state(document, NULL, paragraphs, paragraph_count, contents);
        return NULL;
    }


    for (size_t i = 0; i < paragraph_count; i++) {
        char* normalized = normalize_text(paragraphs[i]);
        if (!normalized || strlen(normalized) == 0) {
            free(normalized);
            continue;
        }

        // FILTRO 1: JUNK DETECTION
        if (is_text_junk(normalized)) {
            free(normalized);
            continue;
        }

        block_type_t type = determine_block_type(normalized);

        // FILTRO 2: TABLE DETECTION
        if (type == BLOCK_TYPE_TEXT && is_likely_table(normalized)) {
            type = BLOCK_TYPE_TABLE;
        }

        // FILTRO 3: LATEX CONVERSION
        char* final_content = normalized;
        if (type != BLOCK_TYPE_TABLE) {
            raw_text_block_t temp_block = {0};
            temp_block.content = normalized;
            char* latex_content = block_to_latex(&temp_block);
            if (latex_content) {
                final_content = latex_content;
                type = BLOCK_TYPE_MATH;
                free(normalized);
            }
        }

        size_t line_start = line_numbers[i];
        size_t line_end = (i + 1 < paragraph_count) ? line_numbers[i + 1] - 1 : line_numbers[paragraph_count];

        // Per TXT usiamo create_block_txt (senza bbox)
        raw_text_block_t* block = create_block_txt(final_content, line_start, line_end, type);
        if (final_content != normalized) free(final_content);
        else free(normalized);

        if (!block) {
            DEBUG_LOG("Failed to create block for paragraph %zu.", i);
            continue;
        }

        if (add_block_to_page(page, block, document->id) != 0) {
            DEBUG_LOG("Failed to add block %zu to page.", i);
            free_block(block);
        }
    }

    //Aggiungi pagina al documento
    if (add_page_to_document(document, page) != 0) {
        DEBUG_LOG("Failed to add page to document.");
        free(line_numbers);
        cleanup_parse_state(document, page, paragraphs, paragraph_count, contents);
        return NULL;
    }

    //Deduplicazione (rimuove blocchi identici consecutivi )
    if (page->block_count > 1) {
        size_t w = 0;
        for (size_t i = 1; i < page->block_count; i++) {
            raw_text_block_t* prev = page->blocks[w];
            raw_text_block_t* curr = page->blocks[i];
            
            if (prev && curr && prev->content && curr->content &&
                strcmp(prev->content, curr->content) == 0) {
                free_block(curr);
            } else {
                w++;
                page->blocks[w] = curr;
            }
        }
        page->block_count = w + 1;
    }

    // 10. Registra documento
    registry_register(document->id, filepath);

    // 11. Cleanup
    free(line_numbers);
    cleanup_parse_state(NULL, NULL, paragraphs, paragraph_count, contents);

    DEBUG_LOG("Successfully parsed TXT with %zu blocks", page->block_count);
    set_error(ERROR_ALL_OK);
    return document;
}