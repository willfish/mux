#include "project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void project_init(Project *p) {
    memset(p, 0, sizeof(Project));
    p->attach = true;
    p->startup_pane = -1;
}

void project_free(Project *p) {
    /* When using arena allocation, this is a no-op since the arena
     * owns all the memory. This function exists for the case where
     * manual allocation is used. */
    (void)p;
}

void project_dump(const Project *p) {
    printf("Project: %s\n", p->name ? p->name : "(unnamed)");
    printf("  root: %s\n", p->root ? p->root : "(none)");
    printf("  tmux_command: %s\n", p->tmux_command ? p->tmux_command : "tmux");
    printf("  tmux_options: %s\n", p->tmux_options ? p->tmux_options : "(none)");
    printf("  socket_name: %s\n", p->socket_name ? p->socket_name : "(none)");
    printf("  socket_path: %s\n", p->socket_path ? p->socket_path : "(none)");
    printf("  pre_window: %s\n", p->pre_window ? p->pre_window : "(none)");
    printf("  startup_window: %s\n", p->startup_window ? p->startup_window : "(none)");
    printf("  startup_pane: %d\n", p->startup_pane);
    printf("  attach: %s\n", p->attach ? "true" : "false");
    printf("  enable_pane_titles: %s\n", p->enable_pane_titles ? "true" : "false");
    printf("  pane_title_format: %s\n", p->pane_title_format ? p->pane_title_format : "(none)");
    printf("  pane_title_position: %s\n",
           p->pane_title_position ? p->pane_title_position : "(none)");

    if (p->on_project_start)
        printf("  on_project_start: %s\n", p->on_project_start);
    if (p->on_project_first_start)
        printf("  on_project_first_start: %s\n", p->on_project_first_start);
    if (p->on_project_restart)
        printf("  on_project_restart: %s\n", p->on_project_restart);
    if (p->on_project_exit)
        printf("  on_project_exit: %s\n", p->on_project_exit);
    if (p->on_project_stop)
        printf("  on_project_stop: %s\n", p->on_project_stop);

    printf("  windows (%d):\n", p->window_count);
    for (int i = 0; i < p->window_count; i++) {
        Window *w = &p->windows[i];
        printf("    [%d] %s\n", i, w->name ? w->name : "(unnamed)");
        if (w->root)
            printf("      root: %s\n", w->root);
        if (w->layout)
            printf("      layout: %s\n", w->layout);
        if (w->pre)
            printf("      pre: %s\n", w->pre);
        if (w->synchronize)
            printf("      synchronize: %s\n", w->synchronize);
        printf("      panes (%d):\n", w->pane_count);
        for (int j = 0; j < w->pane_count; j++) {
            Pane *pn = &w->panes[j];
            if (pn->title) {
                printf("        [%d] (title: %s)\n", j, pn->title);
            } else {
                printf("        [%d]\n", j);
            }
            for (int k = 0; k < pn->command_count; k++) {
                printf("          cmd: %s\n", pn->commands[k]);
            }
        }
    }
}
