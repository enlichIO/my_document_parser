
#include "string_utils.h"
#include "error.h"
#include "registry.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    const char* filepath = "lorem.txt";
    printf("=== Starting String Utils Test ===\n\n");

  
    printf("1. Reading file: %s\n", filepath);
    char* contents = read_file_contents(filepath);
    if (!contents) {
        printf("   FAILED: %s\n", parser_error_to_string(get_error()));
        return 1;
    }
    printf("   SUCCESS: Read %zu bytes.\n\n", strlen(contents));

  
    bool use_double = double_newlines_parag(contents);
    printf("2. Heuristic check: Use double newlines? %s\n\n", use_double ? "YES" : "NO");

    // 3. Split into paragraphs
    size_t count = 0;
    char** paragraphs = split_into_paragraphs(contents, &count, use_double);
    if (!paragraphs) {
        printf("   FAILED: %s\n", parser_error_to_string(get_error()));
        free(contents);
        return 1;
    }
    printf("3. Splitting SUCCESS: Found %zu paragraphs.\n\n", count);


    printf("4. Processing paragraphs:\n");
    printf("--------------------------------------------------\n");
    for (size_t i = 0; i < count; i++) {
        // Normalize
        char* clean = normalize_text(paragraphs[i]);
        if (!clean) {
            printf("   [!] Normalization failed for paragraph %zu\n", i);
            continue;
        }

        // Classify
        block_type_t type = determine_block_type(clean);
        
        // Map enum to string for printing
        const char* type_str = "UNKNOWN";
        if (type == BLOCK_TYPE_TEXT) type_str = "TEXT";
        else if (type == BLOCK_TYPE_HEADING) type_str = "HEADING";
        else if (type == BLOCK_TYPE_MATH) type_str = "MATH";
        else if (type == BLOCK_TYPE_CODEBLOCK) type_str = "CODEBLOCK";
        else if (type == BLOCK_TYPE_BLANK) type_str = "BLANK";

        // Print result (truncate long lines for readability)
        printf("Block %-2zu [%-9s]: ", i, type_str);
        if (strlen(clean) > 60) {
            printf("%.57s...\n", clean);
        } else {
            printf("'%s'\n", clean);
        }

        // Free the normalized string (we are done with it)
        free(clean);
    }
    printf("--------------------------------------------------\n\n");

    printf("5. Cleaning up memory...\n");
    for (size_t i = 0; i < count; i++) {
        free(paragraphs[i]); // Free each paragraph string
    }
    free(paragraphs);        // Free the array of pointers
    free(contents);          // Free the original file contents

    printf("   SUCCESS: All memory freed.\n");
    printf("=== Test Completed Successfully ===\n");
    
    return 0;
}