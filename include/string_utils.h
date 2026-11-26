#ifndef STRING_UTILS_H
#define STRING_UTILS_H
#include <string.h>
#include "../include/types.h"
#include <stdbool.h>
#include <stddef.h>

#include <stdbool.h> /* For bool type */
#include <stddef.h>  /* For size_t */

//Returns a normalized copy of the input string.Trims leading/trailing whitespace and collapses internal whitespace, Returns NULL if input is NULL or memory allocation fails.
char* normalize_text(const char* input);

block_type_t determine_block_type(const char* text);

// ""at first look"" check to see if a string contains latex notation,Returns true if math-like content is detected, false otherwise. 
bool looks_like_math(const char* content);

// Reads an entire file into a null-terminated string, Returns NULL if the file cannot be opened or memory allocation fails.
char* read_file_contents(const char* filepath);
//quick check if it uses double newlines to separate paragraphs, Returns true if double newlines density is found, false otherwise.
bool double_newlines_parag(const char* text);

char** split_into_paragraphs(const char* text, size_t* count, bool use_double_newline);

// Genera un ID univoco per il documento basato su nome file e hash del contenuto
char* generate_document_id(const char* filepath, const char* content, size_t content_len);

// Calcola la similarità tra due stringhe (0.0 a 1.0)
double string_similarity(const char* s1, const char* s2);

// Rimuove blocchi duplicati all'interno di una pagina (soglia 0.0 - 1.0)
void deduplicate_page_blocks(parsed_page_t* page, double threshold);

#endif // STRING_UTILS_H