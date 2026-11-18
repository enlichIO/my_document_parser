#include "types.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

raw_text_block_t* create_block(const char* content, int page_number, block_type_t type){
     raw_text_block_t* block = (raw_text_block_t*)malloc(sizeof(raw_text_block_t));
     if (!block) {
         fprintf(stderr, "Memory allocation code not_a_block\n");
         return NULL;
     }
           block->id = NULL;
           block->content = NULL;

    // Duplicate the string
    block->content = strdup(content);
    if (block->content == NULL) {
        free(block); //string failed
        fprintf(stderr, "Memory allocation code no_block_content, page %d\n", page_number);
        return NULL;
    }

     block->page_number = page_number;
     block->block_type = type;
     block->confidence = 1.0f;  // default confidence score, can be updated later
     return block;
}

parsed_page_t* create_page(int page_number){
parsed_page_t* page = (parsed_page_t*)malloc(sizeof(parsed_page_t));
     if (!page) {
         fprintf(stderr, "Memory allocation failed code not_a_parsed_page_t\n");
         return NULL;
     }
     page->page_number = page_number;
     page->block_count = 0;
     page->blocks = NULL;  // No blocks yet
     return page;
}

parsed_document_t* create_document(const char* source_path){
     
     parsed_document_t* doc = (parsed_document_t*)malloc(sizeof(parsed_document_t));
     if (!doc) {
         fprintf(stderr, "Memory allocation failed code not_a_parsed_document_t\n");
         return NULL;
     }
     doc->id = NULL;  //for future possible updates, we can generate a unique id here if needed
     doc->source_file_path = strdup(source_path);
     if (doc->source_file_path == NULL) {
         free(doc);
         fprintf(stderr, "Memory allocation failed code no_source_file_path\n");
         return NULL;
     }
     doc->pages = NULL;  // No pages yet
     doc->page_count = 0;
     return doc;
}

void free_doc(parsed_document_t* doc){
    if (!doc) {fprintf(stderr, "Attempted to free a non-document pointer\n");
     return;}
    if (doc->source_file_path) free(doc->source_file_path);
    if (doc->id) free(doc->id);
    if (doc->pages) {
        for (size_t i = 0; i < doc->page_count; i++) {
            parsed_page_t* page = doc->pages[i];
            if (!page) continue; 
            if (page->blocks) {
                for (size_t j = 0; j < page->block_count; j++) {
                    raw_text_block_t* block = page->blocks[j];
                    if (!block) continue;
                    if (block->id) free(block->id);
                    if (block->content) free(block->content);
                    
                    free(block);
                }
                free(page->blocks);
            }

            free(page);
        }
        free(doc->pages);
    }
    free(doc);
}
