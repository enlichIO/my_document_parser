#ifndef EPUB_PARSER_H
#define EPUB_PARSER_H

#include "types.h"

/**
 * Parses an EPUB file into a structured document.
 */
parsed_document_t* parse_epub_file(const char* filepath);

#endif /* EPUB_PARSER_H */