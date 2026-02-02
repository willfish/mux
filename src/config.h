#ifndef MUX_CONFIG_H
#define MUX_CONFIG_H

#include "arena.h"
#include "project.h"

/* Parse a YAML config file into a Project struct.
 * settings is an array of key/value pairs for template substitution (can be NULL).
 * Returns 0 on success, -1 on error. */
int config_parse(Arena *a, const char *filepath, Project *p, const char **settings,
                 int setting_count);

/* Parse a YAML string into a Project struct.
 * Returns 0 on success, -1 on error. */
int config_parse_string(Arena *a, const char *yaml, size_t yaml_len, Project *p,
                        const char **settings, int setting_count);

#endif
