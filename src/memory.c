#include "../include/types.h"
#include "../include/memory.h"
#include "../include/error.h" // Include error handling header
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
Create a raw text block.
Note: ID is not generated here; it is generated when added to a page.
 */
raw_text_block_t* create_block(const char* content, size_t page_number, block_type_t type, size_t line_start, size_t line_end) {
    if (!content) {
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    raw_text_block_t* block = (raw_text_block_t*)malloc(sizeof(raw_text_block_t));
    if (!block) {
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    // Initialize fields to safe defaults
    block->id = NULL;
    block->content = NULL;
    block->type = type;
    block->confidence = 1.0f; // Default for TXT files
    block->page_number = page_number;
    block->line_start = line_start;
    block->line_end = line_end;

    // Duplicate content string
    block->content = strdup(content);
    if (!block->content) {
        free(block);
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    set_error(ERROR_ALL_OK);
    return block;
}

//  Create an empty page.
 
parsed_page_t* create_page(size_t page_number) {
    parsed_page_t* page = (parsed_page_t*)malloc(sizeof(parsed_page_t));
    if (!page) {
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    page->page_number = page_number;
    page->block_count = 0;
    page->block_capacity = 0; // Start with 0, allocate on first add
    page->blocks = NULL;

    set_error(ERROR_ALL_OK);
    return page;
}

// Create an empty document.
 
parsed_document_t* create_document(const char* source_path) {
    if (!source_path) {
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    parsed_document_t* doc = (parsed_document_t*)malloc(sizeof(parsed_document_t));
    if (!doc) {
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    doc->id = NULL; // Could generate a UUID here if needed
    doc->source_file_path = strdup(source_path);
    if (!doc->source_file_path) {
        free(doc);
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    doc->page_count = 0;
    doc->page_capacity = 0;
    doc->pages = NULL;

    set_error(ERROR_ALL_OK);
    return doc;
}

// Add a block to a page. Generates the block ID.
 
int add_block_to_page(parsed_page_t* page, raw_text_block_t* block, const char* doc_id) {
    if (!page || !block || !doc_id) {
        set_error(ERROR_INVALID_FORMAT);
        return -1;
    }

    // 1. Grow array if necessary
    if (page->block_count >= page->block_capacity) {
        size_t new_capacity = (page->block_capacity == 0) ? 4 : page->block_capacity * 2;
        raw_text_block_t** new_blocks = (raw_text_block_t**)realloc(page->blocks, new_capacity * sizeof(raw_text_block_t*));
        
        if (!new_blocks) {
            set_error(ERROR_MEMORY_ALLOCATION);
            return -1;
        }
        
        page->blocks = new_blocks;
        page->block_capacity = new_capacity;
    }

    // 2. Generate Block ID: "{doc_id}_p{page}_b{index}"
    char id_buffer[256];
    int written = snprintf(id_buffer, sizeof(id_buffer), "%s_p%zu_b%zu", 
                           doc_id, page->page_number, page->block_count);
    
    if (written < 0 || written >= (int)sizeof(id_buffer)) {
        set_error(ERROR_UNKNOWN); // ID generation failed
        return -1;
    }

    block->id = strdup(id_buffer);
    if (!block->id) {
        set_error(ERROR_MEMORY_ALLOCATION);
        return -1;
    }

    // 3. Add block to array
    page->blocks[page->block_count] = block;
    page->block_count++;

    set_error(ERROR_ALL_OK);
    return 0;
}

// Add a page to a document.
 
int add_page_to_document(parsed_document_t* doc, parsed_page_t* page) {
    if (!doc || !page) {
        set_error(ERROR_INVALID_FORMAT);
        return -1;
    }

    // 1. Grow array if necessary
    if (doc->page_count >= doc->page_capacity) {
        size_t new_capacity = (doc->page_capacity == 0) ? 4 : doc->page_capacity * 2;
        parsed_page_t** new_pages = (parsed_page_t**)realloc(doc->pages, new_capacity * sizeof(parsed_page_t*));
        
        if (!new_pages) {
            set_error(ERROR_MEMORY_ALLOCATION);
            return -1;
        }
        
        doc->pages = new_pages;
        doc->page_capacity = new_capacity;
    }

    // 2. Add page to array
    doc->pages[doc->page_count] = page;
    doc->page_count++;

    set_error(ERROR_ALL_OK);
    return 0;
}

// Free entire document hierarchy recursively
void free_doc(parsed_document_t* doc) {
    if (!doc) return;

    // Free source path
    if (doc->source_file_path) {
        free(doc->source_file_path);
        doc->source_file_path = NULL;
    }

    // Free document ID
    if (doc->id) {
        free(doc->id);
        doc->id = NULL;
    }

    // Free pages
    if (doc->pages) {
        for (size_t i = 0; i < doc->page_count; i++) {
            parsed_page_t* page = doc->pages[i];
            if (!page) continue;

            // Free blocks in this page
            if (page->blocks) {
                for (size_t j = 0; j < page->block_count; j++) {
                    raw_text_block_t* block = page->blocks[j];
                    if (!block) continue;

                    if (block->id) {
                        free(block->id);
                        block->id = NULL;
                    }
                    if (block->content) {
                        free(block->content);
                        block->content = NULL;
                    }
                    
                    free(block);
                }
                free(page->blocks);
                page->blocks = NULL;
            }

            free(page);
        }
        free(doc->pages);
        doc->pages = NULL;
    }

    // Free document struct itself
    free(doc);
}

void cleanup_parse_state(parsed_document_t* doc, parsed_page_t* page, char** paragraphs, size_t paragraph_count, char* contents) {
    if (page) {
        for (size_t i = 0; i < page->block_count; i++) {
            raw_text_block_t* block = page->blocks[i];
            if (block) {
                if (block->id) free(block->id);
                if (block->content) free(block->content);
                free(block);
            }
        }
        if (page->blocks) free(page->blocks);
        free(page);
    }

    if (paragraphs) {
        for (size_t i = 0; i < paragraph_count; i++) {
            free(paragraphs[i]);
        }
        free(paragraphs);
    }
    
    if (contents) {
        free(contents);
    }
    
    if (doc) {
        free_doc(doc);
    }
}