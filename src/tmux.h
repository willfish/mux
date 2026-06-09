#ifndef MUX_TMUX_H
#define MUX_TMUX_H

#include "arena.h"

/* Build a shell command that checks whether a tmux session exists. */
char *tmux_has_session_command(Arena *a, const char *session_name);

/* Return 1 when the named tmux session exists, otherwise 0. */
int tmux_has_session(Arena *a, const char *session_name);

/* Parse newline-separated tmux session names. */
char **tmux_parse_session_names(Arena *a, const char *output, int *count);

/* Return 1 when session_names contains an exact session_name match. */
int tmux_session_names_contain(char **session_names, int count, const char *session_name);

/* Return active tmux session names by asking tmux once. */
char **tmux_list_sessions(Arena *a, int *count);

#endif
