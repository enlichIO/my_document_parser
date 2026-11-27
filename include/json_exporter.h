#ifndef JSON_EXPORTER_H
#define JSON_EXPORTER_H

#include "types.h"


int export_document_to_json(const parsed_document_t *doc, const char *output_path);


char* export_document_to_json_string(const parsed_document_t *doc);

#endif /* JSON_EXPORTER_H */