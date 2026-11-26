#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdbool.h>

// Inizializza il registro (chiamare all'avvio del programma)
void registry_init(void);

// Controlla se un documento (tramite ID univoco) è già stato processato
bool registry_is_processed(const char* doc_id);

// Registra un documento come processato
void registry_register(const char* doc_id, const char* filepath);

// Pulisce la memoria del registro (chiamare alla chiusura del programma)
void registry_cleanup(void);

#endif /* REGISTRY_H */