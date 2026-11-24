#ifndef PDF_PARSER_H
#define PDF_PARSER_H

#include "types.h"


parsed_document_t* parse_pdf_file(const char* filepath, const char* password);

#endif /* PDF_PARSER_H */

