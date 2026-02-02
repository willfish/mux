#ifndef MUX_SCRIPT_H
#define MUX_SCRIPT_H

#include "project.h"

/* Generate a bash script to start a tmux session for the given project.
 * Returns a malloc'd string (caller must free). */
char *script_generate_start(const Project *p);

/* Generate a bash script to stop (kill) a tmux session.
 * Returns a malloc'd string (caller must free). */
char *script_generate_stop(const Project *p);

#endif
