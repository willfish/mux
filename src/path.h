#ifndef MUX_PATH_H
#define MUX_PATH_H

#include "arena.h"

/* Return the primary config directory path. Checks in order:
 * 1. $TMUXINATOR_CONFIG (if non-empty)
 * 2. $XDG_CONFIG_HOME/tmuxinator (if XDG_CONFIG_HOME is non-empty and dir exists)
 * 3. ~/.config/tmuxinator (if dir exists)
 * 4. ~/.tmuxinator (if dir exists)
 * Falls back to $XDG_CONFIG_HOME/tmuxinator (if non-empty) or ~/.config/tmuxinator. */
char *path_config_dir(Arena *a);

/* Find a project config file by name across all config directories in precedence order.
 * Checks both <name>.yml and <name>.yaml.
 * Returns arena-allocated path, or NULL if not found. */
char *path_find_project(Arena *a, const char *name);

/* Check for a local .tmuxinator.yml or .tmuxinator.yaml in the current directory.
 * Returns arena-allocated path, or NULL if not found. */
char *path_find_local(Arena *a);

/* List all .yml and .yaml project files across config directories (including subdirectories).
 * Returns a sorted, deduplicated, NULL-terminated array of project names (without extension).
 * count is set to the number of entries. */
char **path_list_projects(Arena *a, int *count);

/* Same as path_list_projects, but restricted to a single directory. Returns
 * NULL only when the directory itself is missing. */
char **path_list_projects_in(Arena *a, const char *base_dir, int *count);

/* Return the full path for a new project config: <config_dir>/<name>.yml */
char *path_project_file(Arena *a, const char *name);

/* Recursively create a directory and any missing parent directories.
 * Returns 0 on success, -1 on error. */
int path_mkdir_p(const char *path);

#endif
