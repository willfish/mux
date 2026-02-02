#ifndef MUX_PROJECT_H
#define MUX_PROJECT_H

#include <stdbool.h>

typedef struct {
    char *title;
    char **commands;
    int command_count;
} Pane;

typedef struct {
    char *name;
    char *root;
    char *layout;
    char *pre;
    char *synchronize; /* "before", "after", or NULL */
    Pane *panes;
    int pane_count;
} Window;

typedef struct {
    char *name;
    char *root;
    char *pre_window;
    char *tmux_command;
    char *tmux_options;
    char *socket_name;
    char *socket_path;
    char *startup_window;
    int startup_pane;
    bool attach;
    bool enable_pane_titles;
    char *pane_title_format;
    char *pane_title_position;

    /* Hooks */
    char *on_project_start;
    char *on_project_first_start;
    char *on_project_restart;
    char *on_project_exit;
    char *on_project_stop;

    Window *windows;
    int window_count;
} Project;

/* Initialise a project with defaults. */
void project_init(Project *p);

/* Free project contents (but not the Project pointer itself). */
void project_free(Project *p);

/* Print project contents to stdout for debugging. */
void project_dump(const Project *p);

#endif
