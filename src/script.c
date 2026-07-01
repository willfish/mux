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

static const char *herdr_split_direction(const Window *w) {
    if (!w->layout || !w->layout[0]) return "down";
    if (strcmp(w->layout, "even-horizontal") == 0) return "right";
    if (strstr(w->layout, "vertical")) return "right";
    return "down";
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

static void append_herdr_cwd_arg(Str *s, const char *cwd) {
    if (cwd && cwd[0]) {
        str_append(s, " --cwd ");
        append_shell_word(s, cwd);
    }
}

static void append_herdr_label_arg(Str *s, const char *label) {
    if (label && label[0]) {
        str_append(s, " --label ");
        append_shell_word(s, label);
    }
}

static void append_herdr_capture_value(Str *s, const char *var_name, const char *json_var,
                                       const char *key) {
    str_appendf(s, "%s=$(printf '%%s\\n' \"$%s\" | mux_herdr_json_value ", var_name, json_var);
    append_shell_word(s, key);
    str_append(s, ")\n");
}

static void append_herdr_send_command(Str *s, const char *pane_var, const char *cmd) {
    str_append(s, "\"$herdr_cmd\" pane send-text \"$");
    str_append(s, pane_var);
    str_append(s, "\" ");
    append_shell_word(s, cmd);
    str_append(s, "\n");
    str_append(s, "\"$herdr_cmd\" pane send-keys \"$");
    str_append(s, pane_var);
    str_append(s, "\" enter\n");
}

char *script_generate_start_herdr(const Project *p) {
    Str s = str_with_capacity(4096);

    str_append(&s, "#!/usr/bin/env bash\n");
    str_append(&s, "# shellcheck disable=SC2016,SC2034\n");
    str_append(&s, "set -euo pipefail\n\n");
    str_append(&s, "herdr_cmd=${MUX_HERDR_COMMAND:-herdr}\n\n");
    str_append(&s, "mux_herdr_need_python() {\n");
    str_append(&s, "  if ! command -v python3 >/dev/null 2>&1; then\n");
    str_append(&s, "    echo \"mux: herdr backend requires python3 for Herdr JSON parsing\" >&2\n");
    str_append(&s, "    exit 1\n");
    str_append(&s, "  fi\n");
    str_append(&s, "}\n\n");
    str_append(&s, "mux_herdr_json_value() {\n");
    str_append(&s, "  python3 -c 'import json, sys\n");
    str_append(&s, "key = sys.argv[1]\n");
    str_append(&s, "doc = json.load(sys.stdin)\n");
    str_append(&s, "def walk(value):\n");
    str_append(&s, "    if isinstance(value, dict):\n");
    str_append(&s, "        if key in value:\n");
    str_append(&s, "            return value[key]\n");
    str_append(&s, "        for child in value.values():\n");
    str_append(&s, "            found = walk(child)\n");
    str_append(&s, "            if found is not None:\n");
    str_append(&s, "                return found\n");
    str_append(&s, "    elif isinstance(value, list):\n");
    str_append(&s, "        for child in value:\n");
    str_append(&s, "            found = walk(child)\n");
    str_append(&s, "            if found is not None:\n");
    str_append(&s, "                return found\n");
    str_append(&s, "    return None\n");
    str_append(&s, "value = walk(doc)\n");
    str_append(&s, "if value is not None:\n");
    str_append(&s, "    print(value)\n");
    str_append(&s, "' \"$1\"\n");
    str_append(&s, "}\n\n");
    str_append(&s, "mux_herdr_workspace_by_label() {\n");
    str_append(&s, "  \"$herdr_cmd\" workspace list | python3 -c 'import json, sys\n");
    str_append(&s, "label = sys.argv[1]\n");
    str_append(&s, "doc = json.load(sys.stdin)\n");
    str_append(&s, "for workspace in doc.get(\"result\", {}).get(\"workspaces\", []):\n");
    str_append(&s, "    if workspace.get(\"label\") == label:\n");
    str_append(&s, "        print(workspace.get(\"workspace_id\", \"\"))\n");
    str_append(&s, "        break\n");
    str_append(&s, "' \"$1\"\n");
    str_append(&s, "}\n\n");
    str_append(&s, "mux_herdr_attach() {\n");
    str_append(&s, "  if [ \"${MUX_HERDR_ATTACH:-1}\" = \"0\" ]; then\n");
    str_append(&s, "    return 0\n");
    str_append(&s, "  fi\n");
    str_append(&s, "  if [ -z \"${HERDR_SESSION:-}\" ] && [ -t 1 ]; then\n");
    str_append(&s, "    \"$herdr_cmd\" session attach default\n");
    str_append(&s, "  fi\n");
    str_append(&s, "}\n\n");
    str_append(&s, "mux_herdr_need_python\n");
    str_append(&s, "if ! \"$herdr_cmd\" status server >/dev/null 2>&1; then\n");
    str_append(
        &s,
        "  echo \"mux: herdr backend requires a running Herdr session; start herdr first\" >&2\n");
    str_append(&s, "  exit 1\n");
    str_append(&s, "fi\n\n");

    if (p->on_project_start && p->on_project_start[0]) {
        str_appendf(&s, "%s\n", p->on_project_start);
    }

    str_append(&s, "workspace_id=$(mux_herdr_workspace_by_label ");
    append_shell_word(&s, p->name);
    str_append(&s, ")\n");
    str_append(&s, "if [ -n \"$workspace_id\" ]; then\n");
    str_append(&s, "  \"$herdr_cmd\" workspace focus \"$workspace_id\" >/dev/null\n");
    str_append(&s, "  mux_herdr_attach\n");
    if (p->on_project_exit && p->on_project_exit[0]) {
        str_appendf(&s, "  %s\n", p->on_project_exit);
    }
    str_append(&s, "  exit 0\n");
    str_append(&s, "fi\n\n");

    if (p->on_project_first_start && p->on_project_first_start[0]) {
        str_appendf(&s, "%s\n\n", p->on_project_first_start);
    }

    const char *first_win_name = (p->window_count > 0) ? p->windows[0].name : "main";
    const char *first_root = (p->window_count > 0) ? window_root(p, &p->windows[0]) : p->root;

    str_append(&s, "workspace_json=$(\"$herdr_cmd\" workspace create --focus");
    append_herdr_cwd_arg(&s, first_root);
    append_herdr_label_arg(&s, p->name);
    str_append(&s, ")\n");
    append_herdr_capture_value(&s, "workspace_id", "workspace_json", "workspace_id");
    append_herdr_capture_value(&s, "tab_0", "workspace_json", "tab_id");
    append_herdr_capture_value(&s, "pane_0_0", "workspace_json", "pane_id");
    str_append(&s, "\"$herdr_cmd\" tab rename \"$tab_0\" ");
    append_shell_word(&s, first_win_name);
    str_append(&s, " >/dev/null\n\n");

    for (int wi = 0; wi < p->window_count; wi++) {
        Window *w = &p->windows[wi];
        const char *wr = window_root(p, w);

        str_appendf(&s, "# Window/tab: %s\n", w->name);
        if (wi > 0) {
            str_appendf(
                &s,
                "tab_json_%d=$(\"$herdr_cmd\" tab create --workspace \"$workspace_id\" --no-focus",
                wi);
            append_herdr_cwd_arg(&s, wr);
            append_herdr_label_arg(&s, w->name);
            str_append(&s, ")\n");
            char json_var[64];
            snprintf(json_var, sizeof(json_var), "tab_json_%d", wi);
            char tab_var[64];
            snprintf(tab_var, sizeof(tab_var), "tab_%d", wi);
            char pane_var[64];
            snprintf(pane_var, sizeof(pane_var), "pane_%d_0", wi);
            append_herdr_capture_value(&s, tab_var, json_var, "tab_id");
            append_herdr_capture_value(&s, pane_var, json_var, "pane_id");
        }

        const char *direction = herdr_split_direction(w);
        for (int pi = 0; pi < w->pane_count; pi++) {
            char pane_var[64];
            snprintf(pane_var, sizeof(pane_var), "pane_%d_%d", wi, pi);
            if (pi > 0) {
                char prev_pane_var[64];
                snprintf(prev_pane_var, sizeof(prev_pane_var), "pane_%d_%d", wi, pi - 1);
                str_appendf(&s,
                            "split_json_%d_%d=$(\"$herdr_cmd\" pane split \"$%s\" --direction %s "
                            "--ratio 0.5 --focus",
                            wi, pi, prev_pane_var, direction);
                append_herdr_cwd_arg(&s, wr);
                str_append(&s, ")\n");
                char json_var[64];
                snprintf(json_var, sizeof(json_var), "split_json_%d_%d", wi, pi);
                append_herdr_capture_value(&s, pane_var, json_var, "pane_id");
            }

            Pane *pn = &w->panes[pi];
            if (pn->title && pn->title[0]) {
                str_append(&s, "\"$herdr_cmd\" pane rename \"$");
                str_append(&s, pane_var);
                str_append(&s, "\" ");
                append_shell_word(&s, pn->title);
                str_append(&s, " >/dev/null\n");
            }
            if (p->pre_window && p->pre_window[0]) {
                append_herdr_send_command(&s, pane_var, p->pre_window);
            }
            if (w->pre && w->pre[0]) {
                append_herdr_send_command(&s, pane_var, w->pre);
            }
            for (int ci = 0; ci < pn->command_count; ci++) {
                append_herdr_send_command(&s, pane_var, pn->commands[ci]);
            }
        }

        int focus_index = focused_pane_index(w);
        if (focus_index >= 0) {
            str_append(&s, "\"$herdr_cmd\" pane focus \"$");
            str_appendf(&s, "pane_%d_%d", wi, focus_index);
            str_append(&s, "\" >/dev/null\n");
        }

        str_append(&s, "\n");
    }

    str_append(&s, "if [ -n \"${workspace_id:-}\" ]; then\n");
    str_append(&s, "  \"$herdr_cmd\" workspace focus \"$workspace_id\" >/dev/null\n");
    str_append(&s, "fi\n");

    if (p->startup_window && p->startup_window[0]) {
        for (int wi = 0; wi < p->window_count; wi++) {
            Window *w = &p->windows[wi];
            if (strcmp(w->name, p->startup_window) == 0) {
                str_appendf(&s, "\"$herdr_cmd\" tab focus \"$tab_%d\" >/dev/null\n", wi);
                if (p->startup_pane >= 0 && p->startup_pane < w->pane_count) {
                    str_appendf(&s, "\"$herdr_cmd\" pane focus \"$pane_%d_%d\" >/dev/null\n", wi,
                                p->startup_pane);
                }
                break;
            }
        }
    } else if (p->startup_pane >= 0 && p->window_count > 0 &&
               p->startup_pane < p->windows[0].pane_count) {
        str_appendf(&s, "\"$herdr_cmd\" pane focus \"$pane_0_%d\" >/dev/null\n", p->startup_pane);
    }

    str_append(&s, "mux_herdr_attach\n");

    if (p->on_project_exit && p->on_project_exit[0]) {
        str_appendf(&s, "\n%s\n", p->on_project_exit);
    }

    char *result = strdup(str_cstr(&s));
    str_free(&s);
    return result;
}

char *script_generate_stop_herdr(const Project *p) {
    Str s = str_with_capacity(1024);

    str_append(&s, "#!/usr/bin/env bash\n");
    str_append(&s, "set -euo pipefail\n\n");
    str_append(&s, "herdr_cmd=${MUX_HERDR_COMMAND:-herdr}\n\n");
    if (p->on_project_stop && p->on_project_stop[0]) {
        str_appendf(&s, "%s\n\n", p->on_project_stop);
    }
    str_append(&s, "if ! command -v python3 >/dev/null 2>&1; then\n");
    str_append(&s, "  echo \"mux: herdr backend requires python3 for Herdr JSON parsing\" >&2\n");
    str_append(&s, "  exit 1\n");
    str_append(&s, "fi\n");
    str_append(&s, "\"$herdr_cmd\" workspace list | python3 -c 'import json, sys\n");
    str_append(&s, "label = sys.argv[1]\n");
    str_append(&s, "doc = json.load(sys.stdin)\n");
    str_append(&s, "for workspace in doc.get(\"result\", {}).get(\"workspaces\", []):\n");
    str_append(&s, "    if workspace.get(\"label\") == label:\n");
    str_append(&s, "        print(workspace.get(\"workspace_id\", \"\"))\n");
    str_append(&s, "' ");
    append_shell_word(&s, p->name);
    str_append(&s, " | while IFS= read -r workspace_id; do\n");
    str_append(&s, "  [ -n \"$workspace_id\" ] || continue\n");
    str_append(&s, "  \"$herdr_cmd\" workspace close \"$workspace_id\" >/dev/null\n");
    str_append(&s, "done\n");

    char *result = strdup(str_cstr(&s));
    str_free(&s);
    return result;
}
