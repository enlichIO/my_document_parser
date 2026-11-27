#ifndef LATEX_CONVERTER_H
#define LATEX_CONVERTER_H

#include "types.h"

char* convert_unicode_to_latex(const char* text);

int detect_math_patterns(parsed_page_t* page);

char* block_to_latex(const raw_text_block_t* block);

#endif /* LATEX_CONVERTER_H */