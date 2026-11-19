#include "../include/error.h"

// Define the global variable
parser_error_t g_last_error = ERROR_ALL_OK;

// Define the functions
void set_error(parser_error_t err) {
    g_last_error = err;
}

parser_error_t get_error(void) {
    return g_last_error;
}

const char* parser_error_to_string(parser_error_t err) {
    switch (err) {
        case ERROR_ALL_OK:
            return "No error occurred.";
        case ERROR_MEMORY_ALLOCATION:
            return "Memory allocation failed.";
        case ERROR_INVALID_FORMAT:
            return "Invalid file format.";
        case ERROR_FILE_NOT_FOUND:
            return "File could not be found.";
        case ERROR_UNKNOWN:
            return "An unknown error occurred.";
        default:
            return "Unrecognized error code.";
    }
}

