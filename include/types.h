#ifndef MY_DOCUMENT_PARSER_TYPES_H
#define MY_DOCUMENT_PARSER_TYPES_H

#include <stddef.h>  /* For size_t */

typedef enum {
    BLOCK_TYPE_UNKNOWN,
    BLOCK_TYPE_TEXT,
    BLOCK_TYPE_MATH,
    BLOCK_TYPE_HEADING,
    BLOCK_TYPE_BLANK,
    BLOCK_TYPE_FOOTER,
    BLOCK_TYPE_CODEBLOCK,
    BLOCK_TYPE_IMAGE,
} block_type_t;
/*
     Represents a raw text block extracted from a document.
     id=Format: "{doc_id}_p{page}_b{index}"
     context=The actual text content
     type=The type of block (text, math, heading, etc.)
     confidence=Confidence score for the block's content (0.0 to 1.0)
     page_number=The page number in the document where the block was taken
     line_start=The starting line of the block in the page
     line_end=The ending line of the block in the page
      
 */
typedef struct {
    char* id;
    char* content;
    block_type_t type;
    float confidence;
    size_t page_number;
    size_t line_start;
    size_t line_end;
} raw_text_block_t;

/*parsed pages structure (page container)
page_id= unique identifier for tracking the page= the filename of the source document 
block_count= the number of text blocks in the page
blocks= an array of pointers to structures representing the blocks in the page
*/
typedef struct parsed_page{
     size_t page_number;
     size_t block_count;
     size_t block_capacity;
     raw_text_block_t** blocks;
} parsed_page_t;

/* parsed document structure (document container)
id= unique identifier for tracking the doc
(redundant)source_filename= the filename of the source doc
source_file_path= the path of the source doc
page_count= the number of pages in the doc
page_capacity= the allocated capacity for the pages array
*/
typedef struct parsed_document {
    char* id;                     /* Unique document ID (UUID format) */
    char* source_file_path;       /* Path to original source file */
    size_t page_count;            /* Number of pages in document */
    size_t page_capacity;         /* Allocated capacity for pages array */
    parsed_page_t** pages;        /* Array of pointers to pages */
} parsed_document_t;

#endif