#ifndef MUX_TMUX_H
#define MUX_TMUX_H

#include "arena.h"

/* Build a shell command that checks whether a tmux session exists. */
char *tmux_has_session_command(Arena *a, const char *session_name);

/* Return 1 when the named tmux session exists, otherwise 0. */
int tmux_has_session(Arena *a, const char *session_name);

#endif
