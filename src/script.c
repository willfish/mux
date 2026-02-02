#include "script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

static const char *tmux_cmd(const Project *p) {
    return (p->tmux_command && p->tmux_command[0]) ? p->tmux_command : "tmux";
}

static void append_tmux_base(Str *s, const Project *p) {
    str_append(s, tmux_cmd(p));
    if (p->socket_name && p->socket_name[0]) {
        str_appendf(s, " -L %s", p->socket_name);
    }
    if (p->socket_path && p->socket_path[0]) {
        str_appendf(s, " -S %s", p->socket_path);
    }
    if (p->tmux_options && p->tmux_options[0]) {
        str_appendf(s, " %s", p->tmux_options);
    }
}

static void append_tmux(Str *s, const Project *p, const char *subcmd) {
    append_tmux_base(s, p);
    str_appendf(s, " %s", subcmd);
}

static void append_send_keys_raw(Str *s, const Project *p, const char *target, const char *cmd) {
    /* Escape double quotes in the command for embedding in bash */
    Str escaped = str_new();
    for (const char *c = cmd; *c; c++) {
        if (*c == '"') {
            str_append(&escaped, "\\\"");
        } else if (*c == '$') {
            str_append(&escaped, "\\$");
        } else if (*c == '`') {
            str_append(&escaped, "\\`");
        } else if (*c == '\\') {
            str_append(&escaped, "\\\\");
        } else {
            str_append_char(&escaped, *c);
        }
    }
    append_tmux_base(s, p);
    str_appendf(s, " send-keys -t %s:%s \"%s\" C-m\n", p->name, target, str_cstr(&escaped));
    str_free(&escaped);
}

static const char *window_root(const Project *p, const Window *w) {
    if (w->root && w->root[0]) return w->root;
    if (p->root && p->root[0]) return p->root;
    return NULL;
}

char *script_generate_start(const Project *p) {
    Str s = str_with_capacity(4096);

    str_append(&s, "#!/usr/bin/env bash\n\n");

    /* on_project_start hook */
    if (p->on_project_start && p->on_project_start[0]) {
        str_appendf(&s, "%s\n", p->on_project_start);
    }

    /* Check if session already exists */
    str_append(&s, "\n# Check if session already exists\n");
    append_tmux_base(&s, p);
    str_appendf(&s, " has-session -t %s 2>/dev/null\n\n", p->name);
    str_append(&s, "if [ $? != 0 ]; then\n\n");

    /* on_project_first_start hook */
    if (p->on_project_first_start && p->on_project_first_start[0]) {
        str_appendf(&s, "%s\n\n", p->on_project_first_start);
    }

    /* Start server */
    append_tmux(&s, p, "start-server\n");

    /* Create new session with first window */
    str_append(&s, "\n# Create new session\n");
    const char *first_win_name = (p->window_count > 0) ? p->windows[0].name : "main";
    const char *first_root = (p->window_count > 0) ? window_root(p, &p->windows[0]) : p->root;

    append_tmux_base(&s, p);
    str_appendf(&s, " new-session -d -s %s -n %s", p->name, first_win_name);
    if (first_root && first_root[0]) {
        str_appendf(&s, " -c %s", first_root);
    }
    str_append(&s, "\n");

    /* Create windows and panes */
    for (int wi = 0; wi < p->window_count; wi++) {
        Window *w = &p->windows[wi];
        const char *wr = window_root(p, w);

        str_appendf(&s, "\n# Window: %s\n", w->name);

        if (wi > 0) {
            /* Create new window */
            append_tmux_base(&s, p);
            str_appendf(&s, " new-window -t %s -n %s", p->name, w->name);
            if (wr && wr[0]) {
                str_appendf(&s, " -c %s", wr);
            }
            str_append(&s, "\n");
        }

        /* Synchronize panes "before" — set sync before sending commands */
        if (w->synchronize && strcmp(w->synchronize, "before") == 0) {
            append_tmux_base(&s, p);
            str_appendf(&s, " set-window-option -t %s:%s synchronize-panes on\n", p->name,
                         w->name);
        }

        /* Create panes (first pane already exists with the window) */
        for (int pi = 0; pi < w->pane_count; pi++) {
            if (pi > 0) {
                append_tmux_base(&s, p);
                str_appendf(&s, " splitw -t %s:%s", p->name, w->name);
                if (wr && wr[0]) {
                    str_appendf(&s, " -c %s", wr);
                }
                str_append(&s, "\n");
            }

            /* Build target: window.pane */
            char target[256];
            snprintf(target, sizeof(target), "%s.%d", w->name, pi);

            /* Pane title */
            if (p->enable_pane_titles && w->panes[pi].title) {
                append_tmux_base(&s, p);
                str_appendf(&s, " select-pane -t %s:%s -T \"%s\"\n", p->name, target,
                             w->panes[pi].title);
            }

            /* pre_window commands */
            if (p->pre_window && p->pre_window[0]) {
                append_send_keys_raw(&s, p, target, p->pre_window);
            }

            /* Window-level pre command */
            if (w->pre && w->pre[0]) {
                append_send_keys_raw(&s, p, target, w->pre);
            }

            /* Pane commands */
            Pane *pn = &w->panes[pi];
            for (int ci = 0; ci < pn->command_count; ci++) {
                append_send_keys_raw(&s, p, target, pn->commands[ci]);
            }
        }

        /* Set layout after all panes are created */
        if (w->layout && w->layout[0]) {
            append_tmux_base(&s, p);
            str_appendf(&s, " select-layout -t %s:%s %s\n", p->name, w->name, w->layout);
        }

        /* Synchronize panes "after" — set sync after sending commands */
        if (w->synchronize && strcmp(w->synchronize, "after") == 0) {
            append_tmux_base(&s, p);
            str_appendf(&s, " set-window-option -t %s:%s synchronize-panes on\n", p->name,
                         w->name);
        }
    }

    /* Enable pane titles globally if configured */
    if (p->enable_pane_titles) {
        str_append(&s, "\n# Pane titles\n");
        append_tmux_base(&s, p);
        str_append(&s, " set-option -g pane-border-status ");
        str_append(&s, p->pane_title_position ? p->pane_title_position : "top");
        str_append(&s, "\n");
        if (p->pane_title_format) {
            append_tmux_base(&s, p);
            str_appendf(&s, " set-option -g pane-border-format \"%s\"\n", p->pane_title_format);
        }
    }

    /* Select startup window */
    str_append(&s, "\n# Select startup window/pane\n");
    if (p->startup_window && p->startup_window[0]) {
        append_tmux_base(&s, p);
        str_appendf(&s, " select-window -t %s:%s\n", p->name, p->startup_window);
    } else {
        append_tmux_base(&s, p);
        str_appendf(&s, " select-window -t %s:%s\n", p->name, first_win_name);
    }

    if (p->startup_pane >= 0) {
        const char *sw =
            (p->startup_window && p->startup_window[0]) ? p->startup_window : first_win_name;
        append_tmux_base(&s, p);
        str_appendf(&s, " select-pane -t %s:%s.%d\n", p->name, sw, p->startup_pane);
    }

    /* End of "session doesn't exist" block */
    if (p->on_project_restart && p->on_project_restart[0]) {
        str_append(&s, "\nelse\n\n");
        str_appendf(&s, "%s\n\n", p->on_project_restart);
    }

    str_append(&s, "\nfi\n\n");

    /* Attach or switch */
    if (p->attach) {
        str_append(&s, "# Attach to session\n");
        str_append(&s, "if [ -z \"$TMUX\" ]; then\n");
        str_append(&s, "  ");
        append_tmux_base(&s, p);
        str_appendf(&s, " -u attach-session -t %s\n", p->name);
        str_append(&s, "else\n");
        str_append(&s, "  ");
        append_tmux_base(&s, p);
        str_appendf(&s, " -u switch-client -t %s\n", p->name);
        str_append(&s, "fi\n");
    }

    /* on_project_exit hook */
    if (p->on_project_exit && p->on_project_exit[0]) {
        str_appendf(&s, "\n%s\n", p->on_project_exit);
    }

    char *result = strdup(str_cstr(&s));
    str_free(&s);
    return result;
}

char *script_generate_stop(const Project *p) {
    Str s = str_with_capacity(512);

    str_append(&s, "#!/usr/bin/env bash\n\n");

    /* on_project_stop hook */
    if (p->on_project_stop && p->on_project_stop[0]) {
        str_appendf(&s, "%s\n\n", p->on_project_stop);
    }

    /* Kill session */
    append_tmux_base(&s, p);
    str_appendf(&s, " kill-session -t %s\n", p->name);

    char *result = strdup(str_cstr(&s));
    str_free(&s);
    return result;
}
