#ifndef MUX_SHELL_H
#define MUX_SHELL_H

#include "arena.h"

/* Shell-escape a string for safe embedding in bash scripts.
 * Returns arena-allocated string wrapped in single quotes. */
char *shell_escape(Arena *a, const char *s);

/* Expand ~ to $HOME at the start of a path. Returns arena-allocated string. */
char *path_expand(Arena *a, const char *path);

/* Execute a command and return its exit status. */
int shell_exec(const char *cmd);

/* Execute a command string via bash. */
int shell_exec_bash(const char *script);

#endif
