#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/epub_parser.h"
#include "../include/memory.h"
#include "../include/error.h"
#include "registry.h"

parsed_document_t* parse_epub_file(const char* filepath);

const char* type_to_string(block_type_t type) {
    switch (type) {
        case BLOCK_TYPE_UNKNOWN: return "UNKNOWN";
        case BLOCK_TYPE_TEXT: return "TEXT";
        case BLOCK_TYPE_MATH: return "MATH";
        case BLOCK_TYPE_HEADING: return "HEADING";
        case BLOCK_TYPE_BLANK: return "BLANK";
        case BLOCK_TYPE_CODEBLOCK: return "CODE";
        default: return "OTHER";
    }
}

int main(int argc, char** argv) {
    const char* filepath = (argc > 1) ? argv[1] : "test.epub";
    
    printf("=== Starting EPUB Parser Test ===\n");
    printf("Parsing: %s\n\n", filepath);

    parsed_document_t* doc = parse_epub_file(filepath);
    
    if (!doc) {
        if (get_error() == ERROR_ALL_OK) {
            printf("ℹ️ Documento già processato in precedenza (Cache Hit).\n");
        } else {
            printf("❌ Failed to parse EPUB. Error: %s\n", parser_error_to_string(get_error()));
        }
        return 1;
    }

    printf("✅ EPUB parsed successfully!\n");
    printf("   Source: %s\n", doc->source_file_path);
    printf("   Unique ID: %s\n", doc->id ? doc->id : "NULL");
    printf("   Total Pages (Chapters): %zu\n\n", doc->page_count);

    size_t limit_pages = (doc->page_count < 5) ? doc->page_count : 5;
    
    for (size_t p = 0; p < limit_pages; p++) {
        parsed_page_t* page = doc->pages[p];
        printf("--- Chapter/Page %zu ---\n", page->page_number);
        
        size_t limit_blocks = (page->block_count < 3) ? page->block_count : 3;
        for (size_t b = 0; b < limit_blocks; b++) {
            raw_text_block_t* block = page->blocks[b];
            printf("  [%s] ID: %s\n", type_to_string(block->type), block->id ? block->id : "N/A");
            
            char preview[85];
            snprintf(preview, sizeof(preview), "%.80s%s", 
                     block->content ? block->content : "NULL", 
                     (block->content && strlen(block->content) > 80) ? "..." : "");
            printf("  Content: \"%s\"\n\n", preview);
        }
        
        if (page->block_count > 3) {
            printf("  ... and %zu more blocks in this chapter.\n\n", page->block_count - 3);
        }
    }

    if (doc->page_count > 5) {
        printf("... (Showing only first 5 chapters of %zu total)\n", doc->page_count);
    }

    free_doc(doc);
    registry_cleanup();
    printf("\n=== Test Finished Successfully (Memory Freed) ===\n");
    return 0;
}