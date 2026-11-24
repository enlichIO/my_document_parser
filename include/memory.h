#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

parsed_document_t* create_document(const char* source_path);
parsed_page_t* create_page(size_t page_number);

// Unified block creation (supports both TXT lines and PDF bounding boxes)
raw_text_block_t* create_block(const char* content, size_t line_start, size_t line_end,int page_number, block_type_t type,float bbox_x0, float bbox_y0, float bbox_x1, float bbox_y1);

// Convenience wrapper for PDF (sets lines to 0)
raw_text_block_t* create_block_pdf(const char* content, int page_number,block_type_t type, float bbox_x0, float bbox_y0,float bbox_x1, float bbox_y1);

// Convenience wrapper for TXT (sets page to -1, bbox to 0)
raw_text_block_t* create_block_txt(const char* content, size_t line_start, size_t line_end,block_type_t type);

int add_block_to_page(parsed_page_t* page, raw_text_block_t* block, const char* doc_id);
int add_page_to_document(parsed_document_t* doc, parsed_page_t* page);

void free_block(raw_text_block_t* block);
void free_page(parsed_page_t* page);
void free_doc(parsed_document_t* doc);

// DYNAMIC CLEANUP: Reusable by ANY parser (TXT, PDF, HTML, etc.)
// Pass NULL for arguments you don't need to free.
void cleanup_parse_state(parsed_document_t* doc, parsed_page_t* page, char** string_array, size_t string_count, char* single_string);

#endif /* MEMORY_H */