#ifndef STRING_UTILS_H
#define STRING_UTILS_H
#include <string.h>
#include "../include/types.h"

#include <stdbool.h> /* For bool type */
#include <stddef.h>  /* For size_t */

/* 
 Generates a unique ID string using an internal counter, Returns NULL if memory allocation fails.
 */
char* generate_id(void);

/* 
 * Returns a normalized copy of the input string.
 Trims leading/trailing whitespace and collapses internal whitespace, Returns NULL if input is NULL or memory allocation fails.
 */
char* normalize_text(const char* input);

/* 
 ""at first look"" check to see if a string contains latex notation,Returns true if math-like content is detected, false otherwise.
 */
bool looks_like_math(const char* content);

/* 
 Reads an entire file into a null-terminated string, Returns NULL if the file cannot be opened or memory allocation fails.
 */
char* read_file_contents(const char* filepath);



#endif // STRING_UTILS_H