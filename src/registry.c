#include "registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_REGISTRY_ENTRIES 1000

typedef struct {
    char* doc_id;
    char* filepath;
} RegistryEntry;

static RegistryEntry registry[MAX_REGISTRY_ENTRIES];
static size_t registry_count = 0;

void registry_init(void) {
    registry_count = 0;
    for (size_t i = 0; i < MAX_REGISTRY_ENTRIES; i++) {
        registry[i].doc_id = NULL;
        registry[i].filepath = NULL;
    }
}

bool registry_is_processed(const char* doc_id) {
    if (!doc_id) return false;
    for (size_t i = 0; i < registry_count; i++) {
        if (registry[i].doc_id && strcmp(registry[i].doc_id, doc_id) == 0) {
            return true;
        }
    }
    return false;
}

void registry_register(const char* doc_id, const char* filepath) {
    if (!doc_id || !filepath) return;
    if (registry_count >= MAX_REGISTRY_ENTRIES) {
        fprintf(stderr, "[REGISTRY] Warning: Registry full!\n");
        return;
    }
    
    if (registry_is_processed(doc_id)) return; // Già presente
    
    registry[registry_count].doc_id = strdup(doc_id);
    registry[registry_count].filepath = strdup(filepath);
    registry_count++;
}

void registry_cleanup(void) {
    for (size_t i = 0; i < registry_count; i++) {
        if (registry[i].doc_id) free(registry[i].doc_id);
        if (registry[i].filepath) free(registry[i].filepath);
    }
    registry_count = 0;
}