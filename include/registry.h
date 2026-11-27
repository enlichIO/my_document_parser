#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdbool.h>

void registry_init(void);
void registry_register(const char* id, const char* filepath);
bool registry_is_processed(const char* id);
void registry_cleanup(void);

#endif