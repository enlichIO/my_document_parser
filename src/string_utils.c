#include "string_utils.h"
#include "error.h"
#include "types.h"
#include "string_utils.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

//oh boy this file has gone out of hand
//will maybe split if too big

// Hash semplice e veloce per generare ID
static unsigned long hash_buffer(const unsigned char* data, size_t length) {
    unsigned long hash = 5381;
    for (size_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    return hash;
}

char* generate_document_id(const char* filepath, const char* content, size_t content_len) {
    if (!filepath) return NULL;
    
    const char* filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
    
    char base_name[256];
    strncpy(base_name, filename, sizeof(base_name) - 1);
    base_name[sizeof(base_name) - 1] = '\0';
    char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0'; // Rimuovi estensione
    
    unsigned long hash_val;
    if (content && content_len > 0) {
        hash_val = hash_buffer((const unsigned char*)content, content_len);
    } else {
        // Fallback per PDF grandi: hash di path + lunghezza (evita di caricare GB in RAM)
        hash_val = hash_buffer((const unsigned char*)filepath, strlen(filepath)) ^ content_len;
    }
    
    char id[512];
    snprintf(id, sizeof(id), "%s_%lx", base_name, hash_val);
    return strdup(id);
}


static void count_newlines_in_range(const char* text, size_t start, size_t end, size_t* double_count, size_t* total_newlines);
char* read_file_contents(const char* filepath) {
    if (!filepath) {
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        set_error(ERROR_FILE_NOT_FOUND);
        return NULL;
    }

    // Get file size
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_error(ERROR_UNKNOWN);
        return NULL;
    }

    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        set_error(ERROR_UNKNOWN);
        return NULL;
    }
    
    rewind(file);

    // Allocate buffer (+1 for null terminator)
    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        fclose(file);
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    // Read content
    size_t bytesRead = fread(buffer, 1, length, file);
    if (bytesRead != (size_t)length) {
        free(buffer);
        fclose(file);
        set_error(ERROR_UNKNOWN);
        return NULL;
    }

    // Null-terminate and cleanup
    buffer[length] = '\0';
    fclose(file);
    
    set_error(ERROR_ALL_OK);
    return buffer;
}

bool double_newlines_parag(const char* text){
    if (!text) return false;
    
    size_t total_length = strlen(text);
    if (total_length == 0) return false;
    
    // Sample 10% of the file
    size_t sample_size = total_length / 10; 
    
    // Minimum: at least 10KB (to get meaningful data)
    if (sample_size < 10000) sample_size = total_length;
    
    // Maximum: at most 500KB
    if (sample_size > 500000) sample_size = 500000;
    
    size_t double_count = 0;
    size_t total_newlines = 0;
    
    // Calculate segment size, ensuring it's at least 1
    size_t segment = sample_size / 3;
    if (segment == 0) segment = 1;

    // Beginning
    count_newlines_in_range(text, 0, segment, &double_count, &total_newlines);
    
    //  Middle
    size_t middle_start = total_length / 2;
    // Ensure we don't go past the end of the string
    size_t middle_end = middle_start + segment;
    if (middle_end > total_length) middle_end = total_length;
    count_newlines_in_range(text, middle_start, middle_end, &double_count, &total_newlines);
    
    //  End
    size_t end_start = total_length - segment;
    if (end_start > 0 && end_start < total_length) {
        count_newlines_in_range(text, end_start, total_length, &double_count, &total_newlines);
    }
    
    if (total_newlines == 0) return false;
    
    // if more than 5% of newlines are doubles, use them
    return ((double)double_count / total_newlines) > 0.05;
}

// Helper function to count newlines in a range
static void count_newlines_in_range(const char* text, size_t start, size_t end, size_t* double_count, size_t* total_newlines) {
    for (size_t i = start; i < end && text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            (*total_newlines)++;
            if (text[i + 1] == '\n') {
                (*double_count)++;
                i++; // Skip next newline
            }
        }
    }
}


char** split_into_paragraphs(const char* text, size_t* count, bool use_double_newline) {
    if (!text || !count) {
        set_error(ERROR_INVALID_FORMAT);
        return NULL;
    }

    // Handle empty text
    if (strlen(text) == 0) {
        *count = 0;
        char** result = (char**)malloc(sizeof(char*));
        if (!result) {
            set_error(ERROR_MEMORY_ALLOCATION);
            return NULL;
        }
        result[0] = NULL;
        set_error(ERROR_ALL_OK);
        return result;
    }

    // Create a temporary copy of the text
    char* copy = strdup(text);
    if (!copy) {   
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    size_t paragraph_count = 0;
    char* saveptr;
    char* token;

    if (use_double_newline) {
        // Count paragraphs for double newlines
        const char* p = text;
        paragraph_count = 1;
        while (*p) {
            if (strncmp(p, "\n\n", 2) == 0) {
                paragraph_count++;
                p += 2;
                continue;
            }
            p++;
        }
    } else {
        // Count paragraphs for single newlines using strtok_r
        char* temp = strdup(text);
        if (!temp) {
            free(copy);
            set_error(ERROR_MEMORY_ALLOCATION);
            return NULL;
        }
        token = strtok_r(temp, "\n", &saveptr);
        while (token) {
            paragraph_count++;
            token = strtok_r(NULL, "\n", &saveptr);
        }
        free(temp);
    }

    // Array allocation for paragraphs + null guard
    char** paragraphs = (char**)malloc((paragraph_count + 1) * sizeof(char*));
    if (!paragraphs) {
        free(copy); // Fixed: was text_copy
        set_error(ERROR_MEMORY_ALLOCATION);
        return NULL;
    }

    size_t idx = 0; // Fixed: added declaration

    if (use_double_newline) {
        char* start = copy; // Fixed: was text_copy
        char* delimiter;
        
        while ((delimiter = strstr(start, "\n\n")) != NULL) {
            *delimiter = '\0'; // Temporarily terminate
            paragraphs[idx] = strdup(start);
            if (!paragraphs[idx]) {
                // Cleanup on failure
                for (size_t k = 0; k < idx; k++) free(paragraphs[k]);
                free(paragraphs);
                free(copy);
                set_error(ERROR_MEMORY_ALLOCATION);
                return NULL;
            }
            idx++;
            start = delimiter + 2; // Skip past both newlines
        }
        
        // Last paragraph (even if empty, e.g., text ends with \n\n)
        paragraphs[idx] = strdup(start);
        if (!paragraphs[idx]) {
            for (size_t k = 0; k < idx; k++) free(paragraphs[k]);
            free(paragraphs);
            free(copy);
            set_error(ERROR_MEMORY_ALLOCATION);
            return NULL;
        }
        idx++;
    } else {
        // Single newline logic using strtok_r
        token = strtok_r(copy, "\n", &saveptr);
        while (token) {
            paragraphs[idx] = strdup(token);
            if (!paragraphs[idx]) {
                for (size_t k = 0; k < idx; k++) free(paragraphs[k]);
                free(paragraphs);
                free(copy);
                set_error(ERROR_MEMORY_ALLOCATION);
                return NULL;
            }
            idx++;
            token = strtok_r(NULL, "\n", &saveptr);
        }
    }

    paragraphs[idx] = NULL; // Sentinel
    *count = idx;
    
    free(copy); // Fixed: was text_copy
    set_error(ERROR_ALL_OK);
    return paragraphs;
}


char* normalize_text(const char* input) {
    if (!input) return NULL;

    // Make a copy
    char* res = strdup(input);
    if (!res) return NULL;

    //  Replace internal newlines/tabs with spaces
    for (char* p = res; *p; p++) {
        if (*p == '\n' || *p == '\t') *p = ' ';
    }

    //  Trim leading whitespace
    char* start = res;
    while (isspace((unsigned char)*start)) start++;

    //  Trim trailing whitespace
    char* end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    
    //  Shift content to the beginning if we trimmed the front
    size_t len = end - start;
    memmove(res, start, len);
    res[len] = '\0';

    return res;
}

bool looks_like_math(const char* content) {
    if (!content) return false;
    
    // 1. Secure math indicators (almost no false positives)
    if (strstr(content, "$$") != NULL) return true;
    if (strstr(content, "\\[") != NULL) return true;
    if (strstr(content, "\\begin{") != NULL) return true;
    
    // 2. Count "math-style" dollar signs
    int math_dollar_count = 0;
    for (const char* p = content; *p != '\0'; p++) {
        if (*p == '$') {
            // Look at the next character
            char next = *(p + 1);
            
            // If next char is a digit, it's likely money ($100)
            if (next >= '0' && next <= '9') {
                continue;
            }
            
            // If next char is end-of-string, it's not math → skip
            if (next == '\0') {
                continue;
            }
            
            // Otherwise, it's likely a math delimiter ($x, $\alpha, etc.)
            math_dollar_count++;
        }
    }
    
    // Need at least 2 math-style dollars to be an equation
    if (math_dollar_count >= 2) {
        return true;
    }

    return false;
}


block_type_t determine_block_type(const char* content) {
    if (!content) return BLOCK_TYPE_UNKNOWN;
    //check for blank content
     const char* p = content;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p == '\0') {
        return BLOCK_TYPE_BLANK;
    }
    // Check for heading indicators
    if (*p == '#') {
        return BLOCK_TYPE_HEADING;
    }

    
    // Check for math indicators first
    if (looks_like_math(content)) {
        return BLOCK_TYPE_MATH;
    }

    // Check for code indicators
    if (strstr(content, "```") != NULL || strstr(content, "~~~") != NULL) {
        return BLOCK_TYPE_CODEBLOCK;
    }

    // Default to text
    return BLOCK_TYPE_TEXT;
}

double string_similarity(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0.0;
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    
    if (len1 == 0 && len2 == 0) return 1.0;
    if (len1 == 0 || len2 == 0) return 0.0;
    
    size_t max_len = (len1 > len2) ? len1 : len2;
    size_t min_len = (len1 < len2) ? len1 : len2;
    
    // Ottimizzazione: se le lunghezze differiscono di oltre il 20%, non possono essere 90% simili
    if ((double)min_len / max_len < 0.8) {
        return 0.0;
    }
    
    // Confronto carattere per carattere (veloce per header/footer allineati)
    size_t matches = 0;
    for (size_t i = 0; i < min_len; i++) {
        if (s1[i] == s2[i]) {
            matches++;
        }
    }
    
    return (double)matches / max_len;
}

void deduplicate_page_blocks(parsed_page_t* page, double threshold) {
    if (!page || page->block_count <= 1) return;
    
    for (size_t i = 0; i < page->block_count; i++) {
        if (!page->blocks[i]) continue;
        
        for (size_t j = i + 1; j < page->block_count; j++) {
            if (!page->blocks[j]) continue;
            
            double sim = string_similarity(page->blocks[i]->content, page->blocks[j]->content);
            if (sim >= threshold) {
                // Blocco duplicato trovato: libera il blocco j
                free_block(page->blocks[j]);
                page->blocks[j] = NULL;
            }
        }
    }
    
    // Compatta l'array rimuovendo i NULL
    size_t write_idx = 0;
    for (size_t i = 0; i < page->block_count; i++) {
        if (page->blocks[i]) {
            page->blocks[write_idx++] = page->blocks[i];
        }
    }
    page->block_count = write_idx;
}