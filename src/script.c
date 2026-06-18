#include "script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

static const char *tmux_cmd(const Project *p) {
    return (p->tmux_command && p->tmux_command[0]) ? p->tmux_command : "tmux";
}

static int shell_word_is_safe(const char *word) {
    if (!word || !word[0]) return 0;

    for (const char *c = word; *c; c++) {
        if ((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9')) {
            continue;
        }
        if (strchr("_./:@%+=,~-", *c)) {
            continue;
        }
        return 0;
    }
    return 1;
}

static void append_shell_word(Str *s, const char *word) {
    if (shell_word_is_safe(word)) {
        str_append(s, word);
        return;
    }

    str_append_char(s, '\'');
    for (const char *c = word ? word : ""; *c; c++) {
        if (*c == '\'') {
            str_append(s, "'\\''");
        } else {
            str_append_char(s, *c);
        }
    }
    str_append_char(s, '\'');
}

static void append_pane_title_arg(Str *s, const char *title) {
    if (shell_word_is_safe(title)) {
        str_appendf(s, "\"%s\"", title);
        return;
    }
    append_shell_word(s, title);
}

static void append_tmux_base(Str *s, const Project *p) {
    append_shell_word(s, tmux_cmd(p));
    if (p->socket_name && p->socket_name[0]) {
        str_append(s, " -L ");
        append_shell_word(s, p->socket_name);
    }
    if (p->socket_path && p->socket_path[0]) {
        str_append(s, " -S ");
        append_shell_word(s, p->socket_path);
    }
    if (p->tmux_options && p->tmux_options[0]) {
        str_appendf(s, " %s", p->tmux_options);
    }
}

static void append_window_target(Str *s, const Project *p, const char *window) {
    append_shell_word(s, p->name);
    str_append_char(s, ':');
    append_shell_word(s, window);
}

static void append_pane_target(Str *s, const Project *p, const char *window, int pane_index) {
    append_window_target(s, p, window);
    str_appendf(s, ".$((pane_base_index+%d))", pane_index);
}

static void append_select_tiled_layout(Str *s, const Project *p, const char *window) {
    append_tmux_base(s, p);
    str_append(s, " select-layout -t ");
    append_window_target(s, p, window);
    str_append(s, " tiled\n");
}

static void append_query_base_indices(Str *s, const Project *p) {
    str_append(s, "pane_base_index=$(");
    append_tmux_base(s, p);
    str_append(s, " show-option -gv pane-base-index 2>/dev/null || echo 0)\n");
    str_append(s, "base_index=$(");
    append_tmux_base(s, p);
    str_append(s, " show-option -gv base-index 2>/dev/null || echo 0)\n");
    str_append(s, "case \"$pane_base_index\" in ''|*[!0-9]*) pane_base_index=0;; esac\n");
    str_append(s, "case \"$base_index\" in ''|*[!0-9]*) base_index=0;; esac\n");
}

static void append_session_target(Str *s, const Project *p) {
    append_shell_word(s, p->name);
}

static void append_send_keys_raw(Str *s, const Project *p, const char *window, int pane_index,
                                 const char *cmd) {
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
    str_append(s, " send-keys -t ");
    append_pane_target(s, p, window, pane_index);
    str_appendf(s, " \"%s\" C-m\n", str_cstr(&escaped));
    str_free(&escaped);
}

static const char *window_root(const Project *p, const Window *w) {
    if (w->root && w->root[0]) return w->root;
    if (p->root && p->root[0]) return p->root;
    return NULL;
}

static int is_nonnegative_int(const char *value) {
    if (!value || !value[0]) return 0;
    for (const char *c = value; *c; c++) {
        if (*c < '0' || *c > '9') return 0;
    }
    return 1;
}

static int focused_pane_index(const Window *w) {
    if (!w->focused_pane || !w->focused_pane[0]) return -1;
    if (is_nonnegative_int(w->focused_pane)) return atoi(w->focused_pane);

    for (int i = 0; i < w->pane_count; i++) {
        if (w->panes[i].title && strcmp(w->panes[i].title, w->focused_pane) == 0) {
            return i;
        }
    }
    return -1;
}

char *script_generate_start(const Project *p) {
    Str s = str_with_capacity(4096);

    str_append(&s, "#!/usr/bin/env bash\n");
    str_append(&s, "set -euo pipefail\n\n");

    /* Query tmux base indices */
    append_tmux_base(&s, p);
    str_append(&s, " start-server\n\n");
    str_append(&s, "# Get tmux base indices\n");
    append_query_base_indices(&s, p);
    str_append(&s, "\n");

    /* on_project_start hook */
    if (p->on_project_start && p->on_project_start[0]) {
        str_appendf(&s, "%s\n", p->on_project_start);
    }

    /* Check if session already exists */
    str_append(&s, "\n# Check if session already exists\n");
    str_append(&s, "if ! ");
    append_tmux_base(&s, p);
    str_append(&s, " has-session -t ");
    append_session_target(&s, p);
    str_append(&s, " 2>/dev/null; then\n\n");

    /* on_project_first_start hook */
    if (p->on_project_first_start && p->on_project_first_start[0]) {
        str_appendf(&s, "%s\n\n", p->on_project_first_start);
    }

    /* Create new session with first window */
    str_append(&s, "\n# Create new session\n");
    const char *first_win_name = (p->window_count > 0) ? p->windows[0].name : "main";
    const char *first_root = (p->window_count > 0) ? window_root(p, &p->windows[0]) : p->root;

    append_tmux_base(&s, p);
    str_append(&s, " new-session -d -s ");
    append_shell_word(&s, p->name);
    str_append(&s, " -x \"${MUX_TMUX_COLUMNS:-120}\" -y \"${MUX_TMUX_LINES:-40}\"");
    str_append(&s, " -n ");
    append_shell_word(&s, first_win_name);
    if (first_root && first_root[0]) {
        str_append(&s, " -c ");
        append_shell_word(&s, first_root);
    }
    str_append(&s, "\n\n");
    str_append(&s, "# Refresh base indices after the first window exists\n");
    append_query_base_indices(&s, p);

    /* Create windows and panes */
    for (int wi = 0; wi < p->window_count; wi++) {
        Window *w = &p->windows[wi];
        const char *wr = window_root(p, w);

        str_appendf(&s, "\n# Window: %s\n", w->name);

        if (wi > 0) {
            /* Create new window */
            append_tmux_base(&s, p);
            str_append(&s, " new-window -t ");
            append_session_target(&s, p);
            str_append(&s, " -n ");
            append_shell_word(&s, w->name);
            if (wr && wr[0]) {
                str_append(&s, " -c ");
                append_shell_word(&s, wr);
            }
            str_append(&s, "\n");
        }

        /* Synchronize panes "before" — set sync before sending commands */
        if (w->synchronize && strcmp(w->synchronize, "before") == 0) {
            append_tmux_base(&s, p);
            str_append(&s, " set-window-option -t ");
            append_window_target(&s, p, w->name);
            str_append(&s, " synchronize-panes on\n");
        }

        /* Create panes (first pane already exists with the window) */
        for (int pi = 0; pi < w->pane_count; pi++) {
            if (pi > 0) {
                append_tmux_base(&s, p);
                str_append(&s, " splitw -t ");
                append_window_target(&s, p, w->name);
                if (wr && wr[0]) {
                    str_append(&s, " -c ");
                    append_shell_word(&s, wr);
                }
                str_append(&s, "\n");
                append_select_tiled_layout(&s, p, w->name);
            }

            /* Pane title */
            if (p->enable_pane_titles && w->panes[pi].title) {
                append_tmux_base(&s, p);
                str_append(&s, " select-pane -t ");
                append_pane_target(&s, p, w->name, pi);
                str_append(&s, " -T ");
                append_pane_title_arg(&s, w->panes[pi].title);
                str_append(&s, "\n");
            }

            /* pre_window commands */
            if (p->pre_window && p->pre_window[0]) {
                append_send_keys_raw(&s, p, w->name, pi, p->pre_window);
            }

            /* Window-level pre command */
            if (w->pre && w->pre[0]) {
                append_send_keys_raw(&s, p, w->name, pi, w->pre);
            }

            /* Pane commands */
            Pane *pn = &w->panes[pi];
            for (int ci = 0; ci < pn->command_count; ci++) {
                append_send_keys_raw(&s, p, w->name, pi, pn->commands[ci]);
            }
        }

        /* Set layout after all panes are created */
        if (w->layout && w->layout[0]) {
            append_tmux_base(&s, p);
            str_append(&s, " select-layout -t ");
            append_window_target(&s, p, w->name);
            str_append_char(&s, ' ');
            append_shell_word(&s, w->layout);
            str_append(&s, "\n");
        }

        int focus_index = focused_pane_index(w);
        if (focus_index >= 0) {
            append_tmux_base(&s, p);
            str_append(&s, " select-pane -t ");
            append_pane_target(&s, p, w->name, focus_index);
            str_append(&s, "\n");
        }

        /* Synchronize panes "after" — set sync after sending commands */
        if (w->synchronize && strcmp(w->synchronize, "after") == 0) {
            append_tmux_base(&s, p);
            str_append(&s, " set-window-option -t ");
            append_window_target(&s, p, w->name);
            str_append(&s, " synchronize-panes on\n");
        }
    }

    /* Enable pane titles globally if configured */
    if (p->enable_pane_titles) {
        str_append(&s, "\n# Pane titles\n");
        append_tmux_base(&s, p);
        str_append(&s, " set-option -g pane-border-status ");
        append_shell_word(&s, p->pane_title_position ? p->pane_title_position : "top");
        str_append(&s, "\n");
        if (p->pane_title_format) {
            append_tmux_base(&s, p);
            str_append(&s, " set-option -g pane-border-format ");
            append_shell_word(&s, p->pane_title_format);
            str_append(&s, "\n");
        }
    }

    /* Select startup window */
    str_append(&s, "\n# Select startup window/pane\n");
    if (p->startup_window && p->startup_window[0]) {
        append_tmux_base(&s, p);
        str_append(&s, " select-window -t ");
        append_window_target(&s, p, p->startup_window);
        str_append(&s, "\n");
    } else {
        append_tmux_base(&s, p);
        str_append(&s, " select-window -t ");
        append_window_target(&s, p, first_win_name);
        str_append(&s, "\n");
    }

    if (p->startup_pane >= 0) {
        const char *sw =
            (p->startup_window && p->startup_window[0]) ? p->startup_window : first_win_name;
        append_tmux_base(&s, p);
        str_append(&s, " select-pane -t ");
        append_pane_target(&s, p, sw, p->startup_pane);
        str_append(&s, "\n");
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
        str_append(&s, "if [ -z \"${TMUX:-}\" ]; then\n");
        str_append(&s, "  ");
        append_tmux_base(&s, p);
        str_append(&s, " -u attach-session -t ");
        append_session_target(&s, p);
        str_append(&s, "\n");
        str_append(&s, "else\n");
        str_append(&s, "  ");
        append_tmux_base(&s, p);
        str_append(&s, " -u switch-client -t ");
        append_session_target(&s, p);
        str_append(&s, "\n");
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
    str_append(&s, " kill-session -t ");
    append_session_target(&s, p);
    str_append(&s, "\n");

    char *result = strdup(str_cstr(&s));
    str_free(&s);
    return result;
}
