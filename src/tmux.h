#ifndef MUX_TMUX_H
#define MUX_TMUX_H

#include "arena.h"
#include "project.h"

/* Replace '.' and ':' with '_' to match tmux's session name sanitisation. */
char *tmux_sanitize_session_name(Arena *a, const char *session_name);

/* Build a shell command that checks whether an exact tmux session exists. */
char *tmux_has_session_command(Arena *a, const char *session_name);

/* Return 1 when the named tmux session exists, otherwise 0. */
int tmux_has_session(Arena *a, const char *session_name);

/* Return 1 when the project's tmux session exists (respecting tmux_command,
 * socket_name, socket_path, and tmux_options). */
int tmux_project_has_session(Arena *a, const Project *p);

/* Parse newline-separated tmux session names into a sorted array. */
char **tmux_parse_session_names(Arena *a, const char *output, int *count);

/* Return 1 when sorted session_names contains an exact session_name match. */
int tmux_session_names_contain(char **session_names, int count, const char *session_name);

/* Return sorted active tmux session names from the default server. */
char **tmux_list_sessions(Arena *a, int *count);

#endif
