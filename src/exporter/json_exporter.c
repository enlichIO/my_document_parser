#include "json_exporter.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

// ============================================================================
// CONFIGURAZIONE CHUNKING
// ============================================================================
#define MAX_CHUNK_SIZE 2000 // Caratteri massimi per chunk (ottimo per Mini-LLMs)

typedef struct {
    char *content;
    size_t len;
    size_t cap;
    int start_page;
    int end_page;
} text_chunk_t;

static bool chunk_init(text_chunk_t *chunk) {
    chunk->cap = 4096;
    chunk->content = malloc(chunk->cap);
    if (!chunk->content) return false;
    
    chunk->len = 0;
    chunk->start_page = -1;
    chunk->end_page = -1;
    chunk->content[0] = '\0';
    return true;
}

static void chunk_free(text_chunk_t *chunk) {
    free(chunk->content);
    chunk->content = NULL;
    chunk->len = 0;
    chunk->cap = 0;
}

static bool chunk_append(text_chunk_t *chunk, const char *text, int page_num) {
    if (!text || !*text) return true;

    size_t text_len = strlen(text);
    bool add_space = chunk->len > 0 && chunk->content[chunk->len - 1] != ' ';
    
    // Controllo overflow
    if (text_len > SIZE_MAX - chunk->len - (add_space ? 2 : 1)) return false;
    
    size_t needed = chunk->len + text_len + (add_space ? 1 : 0) + 1;
    if (needed > chunk->cap) {
        size_t cap = chunk->cap ? chunk->cap : 4096;
        while (cap < needed) {
            if (cap > SIZE_MAX / 2) {
                cap = needed;
                break;
            }
            cap *= 2;
        }
        char *new_content = realloc(chunk->content, cap);
        if (!new_content) return false;
        chunk->content = new_content;
        chunk->cap = cap;
    }

    if (add_space) chunk->content[chunk->len++] = ' ';
    memcpy(chunk->content + chunk->len, text, text_len + 1);
    chunk->len += text_len;
    
    if (chunk->start_page == -1) chunk->start_page = page_num;
    chunk->end_page = page_num;
    
    return true;
}

// ============================================================================
// JSON UTILS
// ============================================================================
static char* json_escape_string(const char *input) {
    if (!input) return strdup("");
    
    size_t len = strlen(input);
    size_t escaped_len = 0;
    
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t') {
            escaped_len += 2;
        } else if (c < 0x20) {
            escaped_len += 6; // \uXXXX
        } else {
            escaped_len += 1;
        }
    }
    
    char *escaped = malloc(escaped_len + 1);
    if (!escaped) return NULL;
    
    char *out = escaped;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input[i];
        switch (c) {
            case '"':  *out++ = '\\'; *out++ = '"'; break;
            case '\\': *out++ = '\\'; *out++ = '\\'; break;
            case '\n': *out++ = '\\'; *out++ = 'n'; break;
            case '\r': *out++ = '\\'; *out++ = 'r'; break;
            case '\t': *out++ = '\\'; *out++ = 't'; break;
            default:
                if (c < 0x20) {
                    sprintf(out, "\\u%04x", c);
                    out += 6;
                } else {
                    *out++ = c;
                }
                break;
        }
    }
    *out = '\0';
    
    return escaped;
}

// ============================================================================
// EXPORT FUNCTION
// ============================================================================
char* export_document_to_json_string(const parsed_document_t *doc) {
    if (!doc) return NULL;

    // Stima iniziale buffer (circa 1KB per pagina come base sicura)
    size_t cap = doc->page_count * 1024 + 4096;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    
    size_t len = 0;
    
    // Header JSON
    char *escaped_id = json_escape_string(doc->id);
    char *escaped_source = json_escape_string(doc->source_file_path);
    
    len += snprintf(buf + len, cap - len, "{\n");
    len += snprintf(buf + len, cap - len, "  \"doc_id\": \"%s\",\n", escaped_id);
    len += snprintf(buf + len, cap - len, "  \"source_file\": \"%s\",\n", escaped_source);
    len += snprintf(buf + len, cap - len, "  \"total_pages\": %zu,\n", doc->page_count);
    len += snprintf(buf + len, cap - len, "  \"chunks\": [\n");
    
    free(escaped_id);
    free(escaped_source);

    text_chunk_t current_chunk;
    if (!chunk_init(&current_chunk)) {
        free(buf);
        return NULL;
    }
    
    int chunk_index = 0;
    bool first_chunk = true;
    char *current_section = strdup("(No Section)");
    if (!current_section) {
        chunk_free(&current_chunk);
        free(buf);
        return NULL;
    }

    for (size_t p = 0; p < doc->page_count; p++) {
        parsed_page_t *page = doc->pages[p];
        
        // Aggiorna sezione se troviamo un header
        for (size_t b = 0; b < page->block_count; b++) {
            if (page->blocks[b]->type == BLOCK_TYPE_HEADING && page->blocks[b]->content) {
                free(current_section);
                current_section = strdup(page->blocks[b]->content);
                if (!current_section) break; // Gestione errore memory
            }
        }
        
        // Processa blocchi
        for (size_t b = 0; b < page->block_count; b++) {
            raw_text_block_t *blk = page->blocks[b];
            
            // Salta blocchi vuoti, troppo corti o UNKNOWN (junk)
            if (!blk->content || strlen(blk->content) < 3) continue;
            if (blk->type == BLOCK_TYPE_UNKNOWN) continue;
            
            // Se il chunk corrente è pieno, scrivilo e iniziane uno nuovo
            if (current_chunk.len + strlen(blk->content) > MAX_CHUNK_SIZE && current_chunk.len > 0) {
                char *escaped_content = json_escape_string(current_chunk.content);
                char *escaped_section = json_escape_string(current_section);
                
                if (escaped_content && escaped_section) {
                    // Stima spazio necessario
                    int needed = snprintf(NULL, 0, 
                        "%s    {\n      \"chunk_id\": %d,\n      \"section_context\": \"%s\",\n      \"content\": \"%s\",\n      \"metadata\": {\n        \"pages\": [%d, %d],\n        \"char_count\": %zu\n      }\n    }",
                        first_chunk ? "" : ",\n",
                        chunk_index,
                        escaped_section,
                        escaped_content,
                        current_chunk.start_page,
                        current_chunk.end_page,
                        current_chunk.len
                    );
                    
                    // Espandi buffer se necessario
                    while (len + (size_t)needed + 1 >= cap) {
                        cap *= 2;
                        char *new_buf = realloc(buf, cap);
                        if (!new_buf) {
                            free(escaped_content);
                            free(escaped_section);
                            chunk_free(&current_chunk);
                            free(current_section);
                            free(buf);
                            return NULL;
                        }
                        buf = new_buf;
                    }
                    
                    len += snprintf(buf + len, cap - len, 
                        "%s    {\n      \"chunk_id\": %d,\n      \"section_context\": \"%s\",\n      \"content\": \"%s\",\n      \"metadata\": {\n        \"pages\": [%d, %d],\n        \"char_count\": %zu\n      }\n    }",
                        first_chunk ? "" : ",\n",
                        chunk_index,
                        escaped_section,
                        escaped_content,
                        current_chunk.start_page,
                        current_chunk.end_page,
                        current_chunk.len
                    );
                    
                    free(escaped_content);
                    free(escaped_section);
                }
                
                chunk_free(&current_chunk);
                chunk_init(&current_chunk);
                chunk_index++;
                first_chunk = false;
            }
            
            chunk_append(&current_chunk, blk->content, (int)p);
        }
    }
    
    // Scrivi l'ultimo chunk rimanente
    if (current_chunk.len > 0) {
        char *escaped_content = json_escape_string(current_chunk.content);
        char *escaped_section = json_escape_string(current_section);
        
        if (escaped_content && escaped_section) {
            int needed = snprintf(NULL, 0, 
                "%s    {\n      \"chunk_id\": %d,\n      \"section_context\": \"%s\",\n      \"content\": \"%s\",\n      \"metadata\": {\n        \"pages\": [%d, %d],\n        \"char_count\": %zu\n      }\n    }",
                first_chunk ? "" : ",\n",
                chunk_index,
                escaped_section,
                escaped_content,
                current_chunk.start_page,
                current_chunk.end_page,
                current_chunk.len
            );
            
            while (len + (size_t)needed + 1 >= cap) {
                cap *= 2;
                char *new_buf = realloc(buf, cap);
                if (!new_buf) {
                    free(escaped_content);
                    free(escaped_section);
                    chunk_free(&current_chunk);
                    free(current_section);
                    free(buf);
                    return NULL;
                }
                buf = new_buf;
            }
            
            len += snprintf(buf + len, cap - len, 
                "%s    {\n      \"chunk_id\": %d,\n      \"section_context\": \"%s\",\n      \"content\": \"%s\",\n      \"metadata\": {\n        \"pages\": [%d, %d],\n        \"char_count\": %zu\n      }\n    }",
                first_chunk ? "" : ",\n",
                chunk_index,
                escaped_section,
                escaped_content,
                current_chunk.start_page,
                current_chunk.end_page,
                current_chunk.len
            );
            
            free(escaped_content);
            free(escaped_section);
        }
    }
    
    len += snprintf(buf + len, cap - len, "\n  ]\n}\n");
    
    chunk_free(&current_chunk);
    free(current_section);
    
    // Ridimensiona alla lunghezza effettiva
    char *final_str = realloc(buf, len + 1);
    return final_str ? final_str : buf;
}

int export_document_to_json(const parsed_document_t *doc, const char *output_path) {
    if (!doc) return -1;
    
    char *json_str = export_document_to_json_string(doc);
    if (!json_str) return -1;
    
    int result = 0;
    
    if (output_path) {
        FILE *fp = fopen(output_path, "w");
        if (!fp) {
            free(json_str);
            return -1;
        }
        
        size_t written = fwrite(json_str, 1, strlen(json_str), fp);
        if (written != strlen(json_str)) {
            result = -1;
        }
        
        fclose(fp);
    } else {
        printf("%s", json_str);
    }
    
    free(json_str);
    return result;
}