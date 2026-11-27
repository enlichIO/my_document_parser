#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define REGISTRY_FILE "registry.json"

typedef struct RegistryEntry {
    char* id;
    char* filepath;
    struct RegistryEntry* next;
} RegistryEntry;

static RegistryEntry* registry_head = NULL;

// Helper per aggiungere alla lista
static void add_entry(const char* id, const char* filepath) {
    RegistryEntry* new_entry = malloc(sizeof(RegistryEntry));
    if (!new_entry) return;
    
    new_entry->id = strdup(id);
    new_entry->filepath = strdup(filepath);
    new_entry->next = registry_head;
    registry_head = new_entry;
}

// Helper per liberare la lista
static void free_list() {
    RegistryEntry* current = registry_head;
    while (current) {
        RegistryEntry* next = current->next;
        free(current->id);
        free(current->filepath);
        free(current);
        current = next;
    }
    registry_head = NULL;
}

void registry_init(void) {
    free_list(); // Pulisci eventuali residui
    
    FILE* fp = fopen(REGISTRY_FILE, "r");
    if (!fp) return; // File non esiste, partiamo da vuoto

    // Parsing JSON molto semplificato per il nostro formato specifico
    // Formato atteso: {"entries": [{"id":"...", "path":"..."}, ...]}
    char buffer[4096];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    buffer[bytes_read] = '\0';

    // Cerchiamo pattern semplici: "id":"VALUE", "path":"VALUE"
    // Nota: Questo è un parser naive ma sufficiente per un registro interno
    char* p = buffer;
    while ((p = strstr(p, "\"id\":\"")) != NULL) {
        p += 6;
        char* id_start = p;
        char* id_end = strchr(p, '"');
        if (!id_end) break;
        
        char* path_marker = strstr(id_end, "\"path\":\"");
        if (!path_marker) break;
        path_marker += 8;
        char* path_start = path_marker;
        char* path_end = strchr(path_marker, '"');
        if (!path_end) break;

        // Estrai stringhe
        size_t id_len = id_end - id_start;
        size_t path_len = path_end - path_start;
        
        char* id = malloc(id_len + 1);
        char* path = malloc(path_len + 1);
        
        if (id && path) {
            memcpy(id, id_start, id_len); id[id_len] = '\0';
            memcpy(path, path_start, path_len); path[path_len] = '\0';
            add_entry(id, path);
            free(id); // La lista fa la sua copia con strdup
            free(path);
        } else {
            free(id); free(path);
        }
        
        p = path_end;
    }
}

void registry_save(void) {
    FILE* fp = fopen(REGISTRY_FILE, "w");
    if (!fp) return;

    fprintf(fp, "{\"entries\":[\n");
    
    RegistryEntry* current = registry_head;
    bool first = true;
    while (current) {
        if (!first) fprintf(fp, ",\n");
        // Escape semplice per JSON (assumiamo path senza quote strane)
        fprintf(fp, "  {\"id\":\"%s\", \"path\":\"%s\"}", current->id, current->filepath);
        first = false;
        current = current->next;
    }
    
    fprintf(fp, "\n]}\n");
    fclose(fp);
}

void registry_cleanup(void) {
    registry_save(); // Salva prima di uscire
    free_list();
}

bool registry_is_processed(const char* id) {
    RegistryEntry* current = registry_head;
    while (current) {
        if (strcmp(current->id, id) == 0) {
            return true;
        }
        current = current->next;
    }
    return false;
}

void registry_register(const char* id, const char* filepath) {
    // Controlla se esiste già per evitare duplicati nella lista
    if (registry_is_processed(id)) return;
    
    add_entry(id, filepath);
}