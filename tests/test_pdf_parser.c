#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <mupdf/fitz.h>
#include "../include/pdf_parser.h"
#include "../include/memory.h"
#include "../include/error.h"

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
    const char* filepath = (argc > 1) ? argv[1] : "test.pdf";
    
    printf("=== Starting PDF Parser Test ===\n");
    printf("Parsing: %s\n\n", filepath);

    parsed_document_t* doc = parse_pdf_file(filepath, NULL);
    
    if (!doc) {
        printf("❌ Failed to parse PDF. Error: %s\n", parser_error_to_string(get_error()));
        return 1;
    }

    printf("✅ PDF parsed successfully!\n");
    printf("   Source: %s\n", doc->source_file_path);
    printf("   ID: %s\n", doc->id ? doc->id : "NULL");
    printf("   Total Pages: %zu\n\n", doc->page_count);

    for (size_t p = 0; p < doc->page_count; p++) {
        parsed_page_t* page = doc->pages[p];
        printf("--- Page %zu ---\n", page->page_number);
        
        size_t limit = (page->block_count < 5) ? page->block_count : 5;
        for (size_t b = 0; b < limit; b++) {
            raw_text_block_t* block = page->blocks[b];
            
            printf("  [%s] ID: %s\n", type_to_string(block->type), block->id ? block->id : "N/A");
            printf("  Location: Page %d | BBox: (%.1f, %.1f) to (%.1f, %.1f)\n", 
                   block->page_number, 
                   block->bbox_x0, block->bbox_y0, 
                   block->bbox_x1, block->bbox_y1);
            
            char preview[85];
            snprintf(preview, sizeof(preview), "%.80s%s", 
                     block->content ? block->content : "NULL", 
                     (block->content && strlen(block->content) > 80) ? "..." : "");
            printf("  Content: \"%s\"\n\n", preview);
        }
        
        if (page->block_count > 5) {
            printf("  ... and %zu more blocks on this page.\n\n", page->block_count - 5);
        }
    }

    free_doc(doc);
    
    printf("=== Test Finished Successfully (Memory Freed) ===\n");
    return 0;
}