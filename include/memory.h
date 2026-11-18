#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

/* create a single block */
raw_text_block_t* create_block(const char* content, int page_number, block_type_t type);

/* make an empty page with no blocks yet, free_doc is responsible for freeing it */
parsed_page_t* create_page(int page_number);

/* root document structure */
parsed_document_t* create_document(const char* source_path);

/* frees the entire document hierarchy recursively */
void free_doc(parsed_document_t* doc);

#endif