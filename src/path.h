#ifndef MUX_PATH_H
#define MUX_PATH_H

#include "arena.h"

/* Return the config directory path. Checks in order:
 * 1. $TMUXINATOR_CONFIG
 * 2. $XDG_CONFIG_HOME/tmuxinator
 * 3. ~/.config/tmuxinator
 * 4. ~/.tmuxinator
 * Returns the first that exists, or the XDG default if none exist. */
char *path_config_dir(Arena *a);

/* Find a project config file by name. Searches config dir for name.yml.
 * Returns arena-allocated path, or NULL if not found. */
char *path_find_project(Arena *a, const char *name);

/* Check for a local .tmuxinator.yml in the current directory.
 * Returns arena-allocated path, or NULL if not found. */
char *path_find_local(Arena *a);

/* List all .yml files in the config directory.
 * Returns a NULL-terminated array of project names (without .yml extension).
 * count is set to the number of entries. */
char **path_list_projects(Arena *a, int *count);

/* Return the full path for a new project config: <config_dir>/<name>.yml */
char *path_project_file(Arena *a, const char *name);

#endif
