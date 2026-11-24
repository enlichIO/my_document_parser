#include "../include/types.h"
#include "../include/memory.h"
#include "../include/error.h" // Include error handling header
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

    doc->id = NULL;
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

// Create an empty page.
parsed_page_t* create_page(size_t page_number) {
    parsed_page_t* page = (parsed_page_t*)malloc(sizeof(parsed_page_t));
    if (!page) {
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    page->page_number = page_number;
    page->block_count = 0;
    page->block_capacity = 0;
    page->blocks = NULL;

    set_error(ERROR_ALL_OK);
    return page;
}

// Create a block with line and/or PDF location metadata.
raw_text_block_t* create_block(const char* content,
                               size_t line_start, size_t line_end,
                               int page_number, block_type_t type,
                               float bbox_x0, float bbox_y0,
                               float bbox_x1, float bbox_y1) {
    if (!content) {
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    raw_text_block_t* block = (raw_text_block_t*)malloc(sizeof(raw_text_block_t));
    if (!block) {
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    block->id = NULL;
    block->content = strdup(content);
    if (!block->content) {
        free(block);
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    block->type = type;
    block->line_start = line_start;
    block->line_end = line_end;
    block->page_number = page_number;
    block->bbox_x0 = bbox_x0;
    block->bbox_y0 = bbox_y0;
    block->bbox_x1 = bbox_x1;
    block->bbox_y1 = bbox_y1;

    set_error(ERROR_ALL_OK);
    return block;
}

// PDF wrapper: line metadata is unused.
raw_text_block_t* create_block_pdf(const char* content, int page_number,
                                   block_type_t type, float bbox_x0, float bbox_y0,
                                   float bbox_x1, float bbox_y1) {
    return create_block(content, 0, 0, page_number, type,
                        bbox_x0, bbox_y0, bbox_x1, bbox_y1);
}

// TXT wrapper: PDF coordinates are unused.
raw_text_block_t* create_block_txt(const char* content, size_t line_start,
                                   size_t line_end, block_type_t type) {
    return create_block(content, line_start, line_end, -1, type,
                        0.0f, 0.0f, 0.0f, 0.0f);
}

// Add a block to a page and generate its ID.
int add_block_to_page(parsed_page_t* page, raw_text_block_t* block, const char* doc_id) {
    if (!page || !block || !doc_id) {
        set_error(ERROR_INVALID_FORMAT);
        return -1;
    }

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

    char id_buffer[256];
    int written = snprintf(id_buffer, sizeof(id_buffer), "%s_p%zu_b%zu", 
                           doc_id, page->page_number, page->block_count);
    
    if (written < 0 || written >= (int)sizeof(id_buffer)) {
        set_error(ERROR_UNKNOWN);
        return -1;
    }

    block->id = strdup(id_buffer);
    if (!block->id) {
        set_error(ERROR_MEMORY_ALLOCATION);
        return -1;
    }

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

    doc->pages[doc->page_count] = page;
    doc->page_count++;

    set_error(ERROR_ALL_OK);
    return 0;
}

void free_block(raw_text_block_t* block) {
    if (!block) return;
    if (block->id) free(block->id);
    if (block->content) free(block->content);
    free(block);
}

void free_page(parsed_page_t* page) {
    if (!page) return;
    for (size_t i = 0; i < page->block_count; i++) {
        free_block(page->blocks[i]);
    }
    if (page->blocks) free(page->blocks);
    free(page);
}

void free_doc(parsed_document_t* doc) {
    if (!doc) return;
    for (size_t i = 0; i < doc->page_count; i++) {
        free_page(doc->pages[i]);
    }
    if (doc->pages) free(doc->pages);
    if (doc->id) free(doc->id);
    if (doc->source_file_path) free(doc->source_file_path);
    free(doc);
}

// Shared cleanup for parser-owned state and temporary allocations.
void cleanup_parse_state(parsed_document_t* doc, parsed_page_t* page,
                         char** string_array, size_t string_count,
                         char* single_string) {
    // Free an unattached page and all blocks it owns.
    if (page) free_page(page);

    // Free the document and its owned pages.
    if (doc) free_doc(doc);

    // Free parser-specific temporary strings.
    if (string_array) {
        for (size_t i = 0; i < string_count; i++) {
            free(string_array[i]);
        }
        free(string_array);
    }

    // Free parser-specific raw contents.
    if (single_string) free(single_string);
}