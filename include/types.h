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
/* raw text block structure
id= unique identifier for tracking
content= the text content of the text
type= the type of the text block (e.g., text, math, heading, etc..)
confidence= the confidence score of the text, from 0,0 to 1, expecially or easy to break rendered latex or images
page_number(ex line_numbber)= the page number of the text block in the source doc
*/
typedef struct {
     char* id;
     int page_number;
     char* content;
     block_type_t block_type;
     float confidence;
} raw_text_block_t;

/*parsed pages structure (page container)
page_id= unique identifier for tracking the page= the filename of the source document 
block_count= the number of text blocks in the page
*/
typedef struct parsed_page{
     int page_number;
     size_t block_count;
     raw_text_block_t** blocks;
} parsed_page_t;

/* parsed document structure (document container)
id= unique identifier for tracking the doc
(redundant)source_filename= the filename of the source doc
source_file_path= the path of the source doc
page_count= the number of pages in the doc
*/
typedef struct parsed_document {
     char* id;
     char* source_file_path;
     parsed_page_t** pages;
     size_t page_count;
} parsed_document_t;

#endif