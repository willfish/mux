#ifndef MUX_TEMPLATE_H
#define MUX_TEMPLATE_H

#include <stddef.h>

#include "arena.h"

/* Substitute <%= @settings["key"] %>, <%= @settings['key'] %>,
 * <%= @args[N] %>, and <%= ENV["VAR"] %> / <%= ENV['VAR'] %> patterns.
 * settings is an array of [key, value, ...] pairs (setting_count = 2 * num_pairs).
 * Returns arena-allocated result string. */
char *template_substitute_ex(Arena *a, const char *input, size_t input_len, const char **settings,
                             int setting_count, const char **args, int arg_count);

/* Substitute template patterns in a NUL-terminated string.
 * Returns arena-allocated result string. */
char *template_substitute(Arena *a, const char *input, const char **settings, int setting_count);

/* Parse "key=value" from a CLI argument. Returns 0 on success, -1 on failure.
 * key and value are set to point into arena-allocated copies. */
int template_parse_setting(Arena *a, const char *arg, char **key, char **value);

#endif
