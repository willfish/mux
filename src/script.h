#ifndef MUX_SCRIPT_H
#define MUX_SCRIPT_H

#include <stdbool.h>

#include "project.h"

typedef struct {
    bool append;
    int attach_override; /* -1 = use project config, 0 = detach, 1 = attach */
    bool no_pre_window;
} ScriptOptions;

/* Generate a bash script to start a tmux session for the given project.
 * Returns a malloc'd string (caller must free). */
char *script_generate_start(const Project *p);
char *script_generate_start_ex(const Project *p, const ScriptOptions *opts);

/* Generate a bash script to start a Herdr workspace for the given project.
 * Experimental: tmux layouts are approximated with Herdr pane splits.
 * Returns a malloc'd string (caller must free). */
char *script_generate_start_herdr(const Project *p);
char *script_generate_start_herdr_ex(const Project *p, const ScriptOptions *opts);

/* Generate a bash script to start a space in the running Herdr desktop app.
 * Returns a malloc'd string (caller must free). */
char *script_generate_start_herdr_app(const Project *p);
char *script_generate_start_herdr_app_ex(const Project *p, const ScriptOptions *opts);

/* Generate a bash script to stop (kill) a tmux session.
 * Returns a malloc'd string (caller must free). */
char *script_generate_stop(const Project *p);

/* Generate a bash script to stop (close) a Herdr workspace by project label.
 * Returns a malloc'd string (caller must free). */
char *script_generate_stop_herdr(const Project *p);

/* Generate a bash script to close a local Herdr desktop space by project name.
 * Returns a malloc'd string (caller must free). */
char *script_generate_stop_herdr_app(const Project *p);

#endif
