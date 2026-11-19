#ifndef ERROR_H
#define ERROR_H


typedef enum parser_error_code {
     ERROR_ALL_OK,              // No error occurred
     ERROR_MEMORY_ALLOCATION,   // Memory allocation failed
     ERROR_INVALID_FORMAT,
     ERROR_FILE_NOT_FOUND,      // File could not be found
     ERROR_UNKNOWN,           // An unknown error occurred
} parser_error_t;

void set_error(parser_error_t err);
parser_error_t get_error(void);
const char* parser_error_to_string(parser_error_t err);

#endif