#ifndef MUX_TEMPLATE_H
#define MUX_TEMPLATE_H

#include "arena.h"

/* Substitute <%= @settings["key"] %> patterns in a string.
 * settings is an array of key=value pairs (NULL-terminated).
 * Returns arena-allocated result string. */
char *template_substitute(Arena *a, const char *input, const char **settings, int setting_count);

/* Parse "key=value" from a CLI argument. Returns 0 on success, -1 on failure.
 * key and value are set to point into arena-allocated copies. */
int template_parse_setting(Arena *a, const char *arg, char **key, char **value);

#endif
