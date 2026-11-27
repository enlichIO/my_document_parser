#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/pdf_parser.h"
#include "../include/txt_parser.h"
#include "../include/json_exporter.h"
#include "../include/memory.h"
#include "../include/error.h"
#include "../include/registry.h"
#include "registry.h"
int main(int argc, char** argv) {
    const char* filepath = (argc > 1) ? argv[1] : "test.pdf";
    const char* output_path = (argc > 2) ? argv[2] : NULL;
    
    printf("=== JSON Export Test ===\n");
    printf("Input:  %s\n", filepath);
    printf("Output: %s\n\n", output_path ? output_path : "stdout");
    
    // Inizializza il registry
    registry_init();
    
    // Determina il tipo di file dall'estensione
    parsed_document_t* doc = NULL;
    const char* ext = strrchr(filepath, '.');
    
    if (ext && strcmp(ext, ".pdf") == 0) {
        doc = parse_pdf_file(filepath, NULL);
    } else if (ext && strcmp(ext, ".txt") == 0) {
        doc = parse_txt_file(filepath);
    } else {
        printf("❌ Formato non supportato. Usa .pdf o .txt\n");
        registry_cleanup();
        return 1;
    }
    
    if (!doc) {
        parser_error_t err = get_error();
        if (err == ERROR_ALL_OK) {
            printf("ℹ️ Documento già processato (cache hit).\n");
        } else {
            printf("❌ Failed to parse document. Error: %s\n", parser_error_to_string(err));
        }
        registry_cleanup();
        return 1;
    }
    
    printf("✅ Parsed successfully. Exporting to JSON...\n\n");
    
    // Esporta in JSON
    if (export_document_to_json(doc, output_path) == 0) {
        if (output_path) {
            printf("✅ JSON exported to: %s\n", output_path);
        }
    } else {
        printf("❌ JSON export failed.\n");
        free_doc(doc);
        registry_cleanup();
        return 1;
    }
    
    // Cleanup
    free_doc(doc);
    registry_cleanup();
    
    printf("\n=== Test Complete ===\n");
    return 0;
}