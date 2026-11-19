#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"
// create a parsed_document_t structure and initialize its fields, Returns NULL if memory allocation fails.
parsed_document_t* create_document(const char* source_path);
// create a parsed_page_t structure and initialize its fields, Returns NULL if memory allocation fails.
parsed_page_t* create_page(size_t page_number);
// create a raw_text_block_t structure and initialize its fields, Returns NULL if memory allocation fails.
raw_text_block_t* create_block(const char* content, size_t page_number,block_type_t type,size_t line_start,size_t line_end);
//create a block to add to a page, Returns NULL if memory allocation fails.
int add_block_to_page(parsed_page_t* page, raw_text_block_t* block, const char* doc_id);
//create a page to add to a document
int add_page_to_document(parsed_document_t* doc, parsed_page_t* page);
//recursively free a parsed_document_t structure and all its associated pages and blocks
void free_doc(parsed_document_t* doc);

#endif