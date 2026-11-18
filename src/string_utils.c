#include "../include/string_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static int id_counter = 0;

char* generate_id(void){
     char* id = (char*)malloc(20 * sizeof(char));
     if (!id) {
         return NULL; // Memory allocation failed
     }
     snprintf(id, 20, "id_%d", id_counter);
     return id;
     id_counter++;// start from id 0
}

char* normalize_text(const char* input) {
     if (!input) {
         return NULL; // Return NULL if input is NULL
     }
     size_t len = strlen(input);
     char* normalized = (char*)malloc((len + 1) * sizeof(char));
     if (!normalized) {
         return NULL; // Memory allocation failed
     }
     int spaces = 0;
     bool in_whitespace = true; // Start in whitespace to trim leading spaces

     for (size_t i = 0; i < len; ++i) {
         if (isspace((unsigned char)input[i])) {
             if (!in_whitespace) {
                 normalized[spaces++] = ' '; // Replace multiple spaces with a single space
                 in_whitespace = true;
             }
         } else {
             normalized[spaces++] = input[i];
             in_whitespace = false;
         }
     }

}
