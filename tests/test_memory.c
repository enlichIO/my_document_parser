#include <stdlib.h>  // Required for malloc, free, strdup
#include <string.h>  // Often required for string functions, though strdup is in stdlib on some systems
#include "registry.h"
#include "../include/types.h"
#include "../include/memory.h"
#include "../include/error.h"
#include <stdio.h>

int main() {
    printf("Starting memory test...\n");

    // 1. Create Document
    parsed_document_t* doc = create_document("test.txt");
    if (!doc) {
        printf("Error creating doc: %s\n", parser_error_to_string(get_error()));
        return 1;
    }
    // For testing IDs, let's manually set a simple doc ID
    doc->id = strdup("doc123"); 

    // 2. Create Page
    parsed_page_t* page = create_page(0);
    if (!page) {
        printf("Error creating page: %s\n", parser_error_to_string(get_error()));
        free_doc(doc);
        return 1;
    }

    // 3. Create Blocks
    raw_text_block_t* b1 = create_block("Hello World", 0, BLOCK_TYPE_TEXT, 0, 0);
    raw_text_block_t* b2 = create_block("This is a test.", 0, BLOCK_TYPE_TEXT, 1, 1);

    if (!b1 || !b2) {
        printf("Error creating blocks: %s\n", parser_error_to_string(get_error()));
        free_doc(doc); // This will free page too if we had added it, but we haven't yet
if (b1) {
    free(b1->content);
    free(b1);
}        if(b2) free(b2->content); free(b2);
        free(page->blocks); free(page);
        return 1;
    }

    // 4. Add Blocks to Page
    if (add_block_to_page(page, b1, doc->id) != 0) {
        printf("Error adding block 1: %s\n", parser_error_to_string(get_error()));
    }
    if (add_block_to_page(page, b2, doc->id) != 0) {
        printf("Error adding block 2: %s\n", parser_error_to_string(get_error()));
    }

    // 5. Add Page to Document
    if (add_page_to_document(doc, page) != 0) {
        printf("Error adding page: %s\n", parser_error_to_string(get_error()));
    }

    printf("Block 1 ID: %s\n", b1->id);
    printf("Block 2 ID: %s\n", b2->id);

    // 6. Cleanup
    free_doc(doc);

    printf("Test finished successfully.\n");
    return 0;
}