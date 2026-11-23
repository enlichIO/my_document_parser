#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include "../include/txt_parser.h"
#include "../include/error.h"
#include "../include/types.h"
#include "../include/memory.h"

void test_file(const char* filepath) {
    printf("\n========================================\n");
    printf("Testing: %s\n", filepath);
    printf("========================================\n");

    parsed_document_t* doc = parse_txt_file(filepath);
    
    if (!doc) {
        printf("❌ FAILED - Error: %s\n\n", parser_error_to_string(get_error()));
        return;
    }

    printf("✅ SUCCESS\n");
    printf("   Pages: %zu\n", doc->page_count);
    
    if (doc->page_count > 0) {
        parsed_page_t* page = doc->pages[0];
        printf("   Blocks: %zu\n", page->block_count);
        
        for (size_t i = 0; i < page->block_count && i < 3; i++) {
            raw_text_block_t* block = page->blocks[i];
printf("   Block %zu: [%u] %.50s...\n", 
       i, (unsigned int)block->type, block->content ? block->content : "NULL");        }
        
        if (page->block_count > 3) {
            printf("   ... and %zu more blocks\n", page->block_count - 3);
        }
    }

    free_doc(doc);
    printf("   Memory freed successfully\n");
}

int main() {
    printf("=== STRESS TEST SUITE ===\n");

    test_file("tests/txt_testers/test_data.txt");
    test_file("tests/txt_testers/empty.txt");
    test_file("tests/txt_testers/whitespace_only.txt");
    test_file("tests/txt_testers/newlines_only.txt");
    test_file("tests/txt_testers/trailing_newlines.txt");
    test_file("tests/txt_testers/single_char.txt");
    test_file("tests/txt_testers/windows_crlf.txt");

    printf("\n=== ALL TESTS COMPLETE ===\n");
    return 0;
}