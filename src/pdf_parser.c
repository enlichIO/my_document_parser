#include "pdf_parser.h"
#include "string_utils.h"
#include "memory.h"
#include "error.h"
#include <mupdf.h/fitz.h> // uso api già esistente per leggere pdf
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static char* extract_block_text(fz_stext_block* block) {
    if (!block) {
        return NULL;
    }

    size_t text_length = 0;
    for (int i = 0; i < block->len; i++) {
        fz_stext_line* line = block->lines[i];
        for (int j = 0; j < line->len; j++) {
            fz_stext_span* span = line->spans[j];
            text_length += span->len;
        }
    }

}
