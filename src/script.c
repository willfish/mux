#include "script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "str.h"

#define DEFAULT_PANE_TITLE_FORMAT "#{pane_index}: #{pane_title}"

/* startup_pane value for "present in the config but not a valid index";
 * project_init uses -1 for "not specified". Mirrored in src/config.c. */
#define MUX_STARTUP_PANE_INVALID (-2)

static const char *tmux_cmd(const Project *p) {
    return (p->tmux_command && p->tmux_command[0]) ? p->tmux_command : "tmux";
}

static int shell_word_is_safe(const char *word) {
    if (!word || !word[0]) return 0;

    for (const char *c = word; *c; c++) {
        if ((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9')) {
            continue;
        }
        /* Intentionally excludes '~' so non-path words never trigger tilde expansion. */
        if (strchr("_./:@%+=,-", *c)) {
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

/* Emit a filesystem path for bash, preserving leading ~ expansion even when the
 * remainder of the path contains spaces or shell metacharacters. */
static void append_shell_path(Str *s, const char *path) {
    if (!path || !path[0]) {
        str_append(s, "''");
        return;
    }
    if (strcmp(path, "~") == 0) {
        str_append_char(s, '~');
        return;
    }
    if (path[0] == '~' && path[1] == '/') {
        str_append(s, "~/");
        if (path[2] != '\0') {
            append_shell_word(s, path + 2);
        }
        return;
    }
    if (path[0] == '~' && path[1] != '\0') {
        /* "~user/...": emit the tilde prefix unquoted so bash expands it at
         * runtime, quoting only the remainder of the path. */
        const char *slash = strchr(path, '/');
        if (slash) {
            str_appendn(s, path, (size_t)(slash - path) + 1);
            if (slash[1] != '\0') {
                append_shell_word(s, slash + 1);
            }
        } else {
            str_append(s, path);
        }
        return;
    }
    append_shell_word(s, path);
}

static void append_pane_title_arg(Str *s, const char *title) {
    if (shell_word_is_safe(title)) {
        str_appendf(s, "\"%s\"", title);
        return;
    }
    append_shell_word(s, title);
}

/* Write sanitised tmux session name ('.' and ':' -> '_') into out. */
static void sanitize_tmux_name(const char *name, Str *out) {
    str_clear(out);
    if (!name) return;
    for (const char *c = name; *c; c++) {
        if (*c == '.' || *c == ':') {
            str_append_char(out, '_');
        } else {
            str_append_char(out, *c);
        }
    }
}

static void append_tmux_session_name(Str *s, const Project *p) {
    Str sanitized = str_new();
    sanitize_tmux_name(p->name, &sanitized);
    append_shell_word(s, str_cstr(&sanitized));
    str_free(&sanitized);
}

static void append_session_target(Str *s, const Project *p) {
    Str target = str_new();
    str_append_char(&target, '=');
    if (p->name) {
        for (const char *c = p->name; *c; c++) {
            str_append_char(&target, (*c == '.' || *c == ':') ? '_' : *c);
        }
    }
    append_shell_word(s, str_cstr(&target));
    str_free(&target);
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

/* Return a shallow copy of the project whose root is normalised to an
 * absolute path, resolved against the directory mux was invoked from (like
 * Ruby's File.expand_path). Resolving once, before any `cd`, keeps the
 * script's `cd` and every backend's root arguments in the same directory.
 * `~`, `~/...` and `~user/...` roots are left unexpanded for the shell to
 * handle at runtime. *freed_root receives a strdup'd root to free after
 * generation, or NULL. */
static const Project *script_project_abs_root(const Project *p, Project *storage,
                                              char **freed_root) {
    const char *root = p->root;
    *freed_root = NULL;
    *storage = *p;
    if (!root || !root[0] || root[0] == '/' || root[0] == '~') return storage;

    if (strcmp(root, ".") == 0) {
        root = "";
    } else if (strncmp(root, "./", 2) == 0) {
        root += 2;
    }

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) return storage;

    Str buf = str_new();
    str_append(&buf, cwd);
    if (root[0] != '\0') {
        if (buf.len == 0 || buf.data[buf.len - 1] != '/') {
            str_append_char(&buf, '/');
        }
        str_append(&buf, root);
    }
    char *abs = strdup(str_cstr(&buf));
    str_free(&buf);
    if (!abs) return storage;
    storage->root = abs;
    *freed_root = abs;
    return storage;
}

/* Run a user hook without strict mode so unset variables, non-zero exits and
 * pipes in user code cannot abort the generated script. */
static void append_tmux_hook(Str *s, const char *hook) {
    if (!hook || !hook[0]) return;
    str_append(s, "set +euo pipefail\n");
    str_appendf(s, "%s\n", hook);
    str_append(s, "set -euo pipefail\n");
}

static void append_window_target(Str *s, const Project *p, int window_index) {
    append_session_target(s, p);
    str_appendf(s, ":$((base_index+%d))", window_index);
}

static void append_pane_target(Str *s, const Project *p, int window_index, int pane_index) {
    append_window_target(s, p, window_index);
    str_appendf(s, ".$((pane_base_index+%d))", pane_index);
}

static void append_select_tiled_layout(Str *s, const Project *p, const char *win_var,
                                       int window_index) {
    str_append(s, "mux_try ");
    append_tmux_base(s, p);
    str_append(s, " select-layout -t ");
    if (win_var) {
        str_appendf(s, "\"$%s\"", win_var);
    } else {
        append_window_target(s, p, window_index);
    }
    str_append(s, " tiled\n");
}

static void append_send_keys_raw(Str *s, const Project *p, const char *win_var, int window_index,
                                 int pane_index, const char *cmd) {
    str_append(s, "mux_try ");
    append_tmux_base(s, p);
    str_append(s, " send-keys -t ");
    if (win_var) {
        str_appendf(s, "\"${%s}.$((pane_base_index+%d))\"", win_var, pane_index);
    } else {
        append_pane_target(s, p, window_index, pane_index);
    }
    str_append(s, " \"");
    for (const char *c = cmd; *c; c++) {
        if (*c == '"' || *c == '$' || *c == '`' || *c == '\\') {
            str_append_char(s, '\\');
        } else if (*c == ';' && c[1] == '\0') {
            /* A trailing ';' would be parsed by tmux as a command separator
             * ("unknown command: C-m"). Inside double quotes bash keeps the
             * backslash, so tmux receives the two characters `\;`, its
             * escaped literal semicolon. Interior semicolons are fine. */
            str_append_char(s, '\\');
        }
        str_append_char(s, *c);
    }
    str_append(s, "\" C-m\n");
}

/* Resolve the effective root directory for a window, joining relative window roots
 * onto the project root (matching tmuxinator). Returns 1 if a root was written. */
static int resolve_window_root(const Project *p, const Window *w, Str *out) {
    str_clear(out);
    if (w && w->root && w->root[0]) {
        const char *wr = w->root;
        if (wr[0] == '/' || wr[0] == '~' || !p->root || !p->root[0]) {
            str_append(out, wr);
            return 1;
        }
        /* Relative window root: join onto project root */
        if (strncmp(wr, "./", 2) == 0) {
            wr += 2;
        }
        str_append(out, p->root);
        if (out->len > 0 && out->data[out->len - 1] != '/') {
            str_append_char(out, '/');
        }
        str_append(out, wr);
        return 1;
    }
    if (p->root && p->root[0]) {
        str_append(out, p->root);
        return 1;
    }
    return 0;
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

/* Resolve focused_pane to a valid pane index [0, pane_count - 1].
 * Falls back to 0 when out of range or unresolved. */
static int resolve_focused_pane_index(const Window *w) {
    if (!w || w->pane_count <= 0) return 0;
    if (!w->focused_pane || !w->focused_pane[0]) return 0;

    if (is_nonnegative_int(w->focused_pane)) {
        int idx = atoi(w->focused_pane);
        return (idx >= 0 && idx < w->pane_count) ? idx : 0;
    }

    for (int i = 0; i < w->pane_count; i++) {
        if (w->panes[i].title && strcmp(w->panes[i].title, w->focused_pane) == 0) {
            return i;
        }
    }
    return 0;
}

/* Resolve startup_window by name first, then by 0-based numeric index, else 0. */
static int resolve_startup_window_index(const Project *p) {
    if (!p || p->window_count <= 0) return 0;
    if (!p->startup_window || !p->startup_window[0]) return 0;

    for (int wi = 0; wi < p->window_count; wi++) {
        if (p->windows[wi].name && strcmp(p->windows[wi].name, p->startup_window) == 0) {
            return wi;
        }
    }

    if (is_nonnegative_int(p->startup_window)) {
        int idx = atoi(p->startup_window);
        if (idx >= 0 && idx < p->window_count) {
            return idx;
        }
    }
    return 0;
}

static int effective_attach(const Project *p, const ScriptOptions *opts) {
    if (opts && opts->attach_override >= 0) {
        return opts->attach_override != 0;
    }
    return p->attach ? 1 : 0;
}

static int effective_use_pre_window(const ScriptOptions *opts) {
    return !(opts && opts->no_pre_window);
}

static void append_cd_project_root(Str *s, const Project *p, int safe_cd) {
    if (p->root && p->root[0]) {
        str_append(s, "cd ");
        append_shell_path(s, p->root);
        if (safe_cd) {
            str_append(s, " 2>/dev/null || true");
        }
        str_append(s, "\n");
    }
}

static void append_herdr_hook(Str *s, const char *hook) {
    if (!hook || !hook[0]) return;
    str_append(s, "set +euo pipefail\n");
    str_appendf(s, "%s\n", hook);
    str_append(s, "set -euo pipefail\n");
}

static void emit_tmux_windows(Str *s, const Project *p, int use_pre_window, int append_mode) {
    Str wr = str_new();

    for (int wi = 0; wi < p->window_count; wi++) {
        const Window *w = &p->windows[wi];
        int has_root = resolve_window_root(p, w, &wr);
        char win_var_buf[32];
        const char *win_var = NULL;

        str_appendf(s, "\n# Window: %s\n", (w->name && w->name[0]) ? w->name : "(unnamed)");

        if (append_mode) {
            snprintf(win_var_buf, sizeof(win_var_buf), "win_%d", wi);
            win_var = win_var_buf;
            str_appendf(s, "%s=$(", win_var);
            append_tmux_base(s, p);
            str_append(s, " new-window -P -F '#{window_id}' -t \"=$target_session\"");
            if (w->name && w->name[0]) {
                str_append(s, " -n ");
                append_shell_word(s, w->name);
            }
            if (has_root) {
                str_append(s, " -c ");
                append_shell_path(s, str_cstr(&wr));
            }
            /* Window creation is fatal: appending the remaining windows to a
             * half-built session would silently lose this one. */
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "failed to create window '%s'",
                         (w->name && w->name[0]) ? w->name : "(unnamed)");
                str_append(s, ") || mux_abort ");
                append_shell_word(s, msg);
                str_append(s, "\n");
            }
        } else if (wi > 0) {
            str_append(s, "mux_try ");
            append_tmux_base(s, p);
            str_append(s, " new-window -t ");
            append_session_target(s, p);
            if (w->name && w->name[0]) {
                str_append(s, " -n ");
                append_shell_word(s, w->name);
            }
            if (has_root) {
                str_append(s, " -c ");
                append_shell_path(s, str_cstr(&wr));
            }
            str_append(s, "\n");
        }

        /* Window-scoped pane title options */
        if (p->enable_pane_titles) {
            str_append(s, "mux_try ");
            append_tmux_base(s, p);
            str_append(s, " set-window-option -t ");
            if (win_var) {
                str_appendf(s, "\"$%s\"", win_var);
            } else {
                append_window_target(s, p, wi);
            }
            str_append(s, " pane-border-status ");
            append_shell_word(s, p->pane_title_position ? p->pane_title_position : "top");
            str_append(s, "\n");

            str_append(s, "mux_try ");
            append_tmux_base(s, p);
            str_append(s, " set-window-option -t ");
            if (win_var) {
                str_appendf(s, "\"$%s\"", win_var);
            } else {
                append_window_target(s, p, wi);
            }
            str_append(s, " pane-border-format ");
            append_shell_word(s, p->pane_title_format ? p->pane_title_format
                                                      : DEFAULT_PANE_TITLE_FORMAT);
            str_append(s, "\n");
        }

        /* Synchronize panes "before" */
        if (w->synchronize && strcmp(w->synchronize, "before") == 0) {
            str_append(s, "mux_try ");
            append_tmux_base(s, p);
            str_append(s, " set-window-option -t ");
            if (win_var) {
                str_appendf(s, "\"$%s\"", win_var);
            } else {
                append_window_target(s, p, wi);
            }
            str_append(s, " synchronize-panes on\n");
        }

        /* Create panes (first pane already exists with the window) */
        for (int pi = 0; pi < w->pane_count; pi++) {
            if (pi > 0) {
                str_append(s, "mux_try ");
                append_tmux_base(s, p);
                str_append(s, " splitw -t ");
                if (win_var) {
                    str_appendf(s, "\"$%s\"", win_var);
                } else {
                    append_window_target(s, p, wi);
                }
                if (has_root) {
                    str_append(s, " -c ");
                    append_shell_path(s, str_cstr(&wr));
                }
                str_append(s, "\n");
                append_select_tiled_layout(s, p, win_var, wi);
            }

            /* Pane title */
            if (p->enable_pane_titles && w->panes[pi].title) {
                str_append(s, "mux_try ");
                append_tmux_base(s, p);
                str_append(s, " select-pane -t ");
                if (win_var) {
                    str_appendf(s, "\"${%s}.$((pane_base_index+%d))\"", win_var, pi);
                } else {
                    append_pane_target(s, p, wi, pi);
                }
                str_append(s, " -T ");
                append_pane_title_arg(s, w->panes[pi].title);
                str_append(s, "\n");
            }

            /* pre_window commands */
            if (use_pre_window && p->pre_window && p->pre_window[0]) {
                append_send_keys_raw(s, p, win_var, wi, pi, p->pre_window);
            }

            /* Window-level pre command */
            if (w->pre && w->pre[0]) {
                append_send_keys_raw(s, p, win_var, wi, pi, w->pre);
            }

            /* Pane commands */
            const Pane *pn = &w->panes[pi];
            for (int ci = 0; ci < pn->command_count; ci++) {
                append_send_keys_raw(s, p, win_var, wi, pi, pn->commands[ci]);
            }
        }

        /* Set layout after all panes are created */
        if (w->layout && w->layout[0]) {
            str_append(s, "mux_try ");
            append_tmux_base(s, p);
            str_append(s, " select-layout -t ");
            if (win_var) {
                str_appendf(s, "\"$%s\"", win_var);
            } else {
                append_window_target(s, p, wi);
            }
            str_append_char(s, ' ');
            append_shell_word(s, w->layout);
            str_append(s, "\n");
        }

        /* Select focused pane (defaults to pane 0 on multi-pane windows, matching tmuxinator) */
        int focus_index = resolve_focused_pane_index(w);
        if (w->pane_count > 1 || (w->focused_pane && w->focused_pane[0])) {
            str_append(s, "mux_try ");
            append_tmux_base(s, p);
            str_append(s, " select-pane -t ");
            if (win_var) {
                str_appendf(s, "\"${%s}.$((pane_base_index+%d))\"", win_var, focus_index);
            } else {
                append_pane_target(s, p, wi, focus_index);
            }
            str_append(s, "\n");
        }

        /* Synchronize panes "after" */
        if (w->synchronize && strcmp(w->synchronize, "after") == 0) {
            str_append(s, "mux_try ");
            append_tmux_base(s, p);
            str_append(s, " set-window-option -t ");
            if (win_var) {
                str_appendf(s, "\"$%s\"", win_var);
            } else {
                append_window_target(s, p, wi);
            }
            str_append(s, " synchronize-panes on\n");
        }
    }

    /* Select startup window / pane */
    int startup_wi = resolve_startup_window_index(p);
    str_append(s, "\n# Select startup window/pane\n");
    str_append(s, "mux_try ");
    append_tmux_base(s, p);
    str_append(s, " select-window -t ");
    if (append_mode) {
        str_appendf(s, "\"$win_%d\"", startup_wi);
    } else {
        append_window_target(s, p, startup_wi);
    }
    str_append(s, "\n");

    int startup_requested = (p->startup_pane >= 0) || (p->startup_pane == MUX_STARTUP_PANE_INVALID);
    if (startup_requested && p->window_count > 0) {
        int sp = (p->startup_pane == MUX_STARTUP_PANE_INVALID) ? 0 : p->startup_pane;
        if (sp >= p->windows[startup_wi].pane_count) {
            sp = 0;
        }
        str_append(s, "mux_try ");
        append_tmux_base(s, p);
        str_append(s, " select-pane -t ");
        if (append_mode) {
            str_appendf(s, "\"${win_%d}.$((pane_base_index+%d))\"", startup_wi, sp);
        } else {
            append_pane_target(s, p, startup_wi, sp);
        }
        str_append(s, "\n");
    }

    str_free(&wr);
}

static void emit_tmux_new_session_body(Str *s, const Project *p, int use_pre_window) {
    /* on_project_first_start hook */
    if (p->on_project_first_start && p->on_project_first_start[0]) {
        append_tmux_hook(s, p->on_project_first_start);
        str_append(s, "\n");
    }

    /* Create new session with first window and capture actual base indices in one call */
    str_append(s, "\n# Create new session\n");
    const char *first_win_name = (p->window_count > 0) ? p->windows[0].name : "main";
    Str first_root = str_new();
    int has_first_root =
        resolve_window_root(p, (p->window_count > 0) ? &p->windows[0] : NULL, &first_root);

    /* Session creation is fatal: with the command substitution inside read's
     * here-string a failure would otherwise be silently masked. */
    str_append(s, "mux_new_session() {\n  ");
    append_tmux_base(s, p);
    str_append(s, " new-session -d -P -F '#{window_index} #{pane_index}' -s ");
    append_tmux_session_name(s, p);
    str_append(s, " -x \"${MUX_TMUX_COLUMNS:-120}\" -y \"${MUX_TMUX_LINES:-40}\"");
    if (first_win_name && first_win_name[0]) {
        str_append(s, " -n ");
        append_shell_word(s, first_win_name);
    }
    if (has_first_root) {
        str_append(s, " -c ");
        append_shell_path(s, str_cstr(&first_root));
    }
    {
        Str sanitized = str_new();
        char msg[256];
        sanitize_tmux_name(p->name, &sanitized);
        snprintf(msg, sizeof(msg), "failed to create tmux session '%s'",
                 (sanitized.len > 0) ? str_cstr(&sanitized) : "(unnamed)");
        str_free(&sanitized);
        /* Retry once: a client can race a server that is exiting after its
         * last session was killed ("server exited unexpectedly"). Errors from
         * the retry are shown; a genuine failure still aborts. */
        str_append(s, "\n}\n");
        str_append(s, "if ! mux_session_info=$(mux_new_session 2>/dev/null) &&\n"
                      "  ! { sleep 0.2; mux_session_info=$(mux_new_session); }; then\n"
                      "  mux_abort ");
        append_shell_word(s, msg);
        str_append(s, "\nfi\n");
    }
    str_append(s, "read -r base_index pane_base_index <<< \"$mux_session_info\"\n");
    str_append(s, "case \"$base_index\" in ''|*[!0-9]*) base_index=0;; esac\n");
    str_append(s, "case \"$pane_base_index\" in ''|*[!0-9]*) pane_base_index=0;; esac\n");

    str_free(&first_root);

    emit_tmux_windows(s, p, use_pre_window, 0);
}

char *script_generate_start_ex(const Project *p, const ScriptOptions *opts) {
    Str s = str_with_capacity(4096);
    int do_attach = effective_attach(p, opts);
    int use_pre_window = effective_use_pre_window(opts);
    int append_mode = (opts && opts->append) ? 1 : 0;

    /* Resolve relative project roots once, before any cd (finding: from /work
     * with `root: app`, the old script cd'd into app/ and then passed the
     * relative root to tmux's -c, resolving to /work/app/app). */
    Project pstorage;
    char *abs_root_alloc = NULL;
    p = script_project_abs_root(p, &pstorage, &abs_root_alloc);

    str_append(&s, "#!/usr/bin/env bash\n");
    str_append(&s, "# shellcheck disable=SC2016\n");
    str_append(&s, "set -euo pipefail\n\n");

    /* Non-fatal tmux failures (layout, select-pane, send-keys, ...) are
     * recorded and make the script exit non-zero at the end, but never abort
     * the build halfway; session/window creation failures abort immediately
     * via mux_abort. */
    str_append(&s, "mux_errors=0\n");
    str_append(&s, "mux_try() {\n");
    str_append(&s, "  \"$@\" || {\n");
    str_append(&s, "    mux_errors=$((mux_errors + 1))\n");
    str_append(&s, "    printf 'mux: tmux command failed: %s\\n' \"$*\" >&2\n");
    str_append(&s, "  }\n");
    str_append(&s, "}\n");
    str_append(&s, "mux_abort() {\n");
    str_append(&s, "  printf 'mux: %s\\n' \"$*\" >&2\n");
    str_append(&s, "  exit 1\n");
    str_append(&s, "}\n\n");

    /* Start tmux server. Best-effort, like tmuxinator: it races with a server
     * that is exiting after its last session was killed ("server exited
     * unexpectedly"), and new-session below starts a server and fails fatally
     * if one genuinely cannot run. */
    append_tmux_base(&s, p);
    str_append(&s, " start-server 2>/dev/null || true\n\n");

    /* Run lifecycle hooks from project root if configured */
    append_cd_project_root(&s, p, 0);

    /* on_project_start hook */
    append_tmux_hook(&s, p->on_project_start);

    if (append_mode) {
        /* --append adds windows to the CURRENT tmux session, matching
         * tmuxinator (template.erb: `if attach? && !append?` also means append
         * mode never attaches or switches). Outside a session there is no
         * current session to append to. */
        str_append(&s, "\n# --append: target the current tmux session\n");
        str_append(&s, "if [ -z \"${TMUX:-}\" ]; then\n");
        str_append(&s, "  mux_abort \"Cannot append to a session that does not exist\"\n");
        str_append(&s, "fi\n");
        str_append(&s, "if ! target_session=$(");
        append_tmux_base(&s, p);
        str_append(&s, " display-message -p '#S' 2>/dev/null); then\n");
        str_append(&s, "  mux_abort \"Cannot append to a session that does not exist\"\n");
        str_append(&s, "fi\n\n");

        str_append(&s, "pane_base_index=$(");
        append_tmux_base(&s, p);
        str_append(&s, " show-option -gv pane-base-index 2>/dev/null || echo 0)\n");
        str_append(&s, "case \"$pane_base_index\" in ''|*[!0-9]*) pane_base_index=0;; esac\n");
        emit_tmux_windows(&s, p, use_pre_window, 1);
    } else {
        /* Check if session already exists (exact match via =name) */
        str_append(&s, "\n# Check if session already exists\n");
        str_append(&s, "if ! ");
        append_tmux_base(&s, p);
        str_append(&s, " has-session -t ");
        append_session_target(&s, p);
        str_append(&s, " 2>/dev/null; then\n\n");

        emit_tmux_new_session_body(&s, p, use_pre_window);

        /* End of "session doesn't exist" block */
        if (p->on_project_restart && p->on_project_restart[0]) {
            str_append(&s, "\nelse\n\n");
            append_tmux_hook(&s, p->on_project_restart);
            str_append(&s, "\n");
        }

        str_append(&s, "\nfi\n\n");

        /* Attach or switch */
        if (do_attach) {
            int can_exec = !(p->on_project_exit && p->on_project_exit[0]);
            str_append(&s, "# Attach to session\n");
            str_append(&s, "if [ -z \"${TMUX:-}\" ]; then\n");
            str_append(&s, can_exec ? "  exec " : "  ");
            append_tmux_base(&s, p);
            str_append(&s, " -u attach-session -t ");
            append_session_target(&s, p);
            str_append(&s, "\n");
            str_append(&s, "else\n");
            str_append(&s, can_exec ? "  exec " : "  ");
            append_tmux_base(&s, p);
            str_append(&s, " -u switch-client -t ");
            append_session_target(&s, p);
            str_append(&s, "\n");
            str_append(&s, "fi\n");
        }
    }

    /* on_project_exit hook */
    if (p->on_project_exit && p->on_project_exit[0]) {
        str_append(&s, "\n");
        append_tmux_hook(&s, p->on_project_exit);
        str_append(&s, "\n");
    }

    /* Recorded non-fatal failures still make the script exit non-zero, after
     * any attach/switch block and hooks. */
    str_append(&s, "\nif [ \"$mux_errors\" -gt 0 ]; then\n");
    str_append(&s, "  mux_abort \"$mux_errors tmux command(s) failed while building the "
                   "session\"\n");
    str_append(&s, "fi\n");

    char *result = strdup(str_cstr(&s));
    str_free(&s);
    free(abs_root_alloc);
    return result;
}

char *script_generate_start(const Project *p) {
    return script_generate_start_ex(p, NULL);
}

char *script_generate_stop(const Project *p) {
    Str s = str_with_capacity(512);
    Project pstorage;
    char *abs_root_alloc = NULL;
    p = script_project_abs_root(p, &pstorage, &abs_root_alloc);

    str_append(&s, "#!/usr/bin/env bash\n\n");

    /* Match tmuxinator's stop template: only cd, run the on_project_stop hook
     * and kill the session when the exact session exists; otherwise exit 0
     * silently instead of failing with tmux's "can't find session" error. */
    str_append(&s, "if ");
    append_tmux_base(&s, p);
    str_append(&s, " has-session -t ");
    append_session_target(&s, p);
    str_append(&s, " 2>/dev/null; then\n\n");

    append_cd_project_root(&s, p, 0);

    /* on_project_stop hook */
    if (p->on_project_stop && p->on_project_stop[0]) {
        str_appendf(&s, "%s\n\n", p->on_project_stop);
    }

    /* Kill session (exact match via =name) */
    append_tmux_base(&s, p);
    str_append(&s, " kill-session -t ");
    append_session_target(&s, p);
    str_append(&s, "\n");

    str_append(&s, "\nfi\n");

    char *result = strdup(str_cstr(&s));
    str_free(&s);
    free(abs_root_alloc);
    return result;
}

static void append_herdr_cwd_arg(Str *s, const char *cwd) {
    if (cwd && cwd[0]) {
        str_append(s, " --cwd ");
        append_shell_path(s, cwd);
    }
}

static void append_herdr_label_arg(Str *s, const char *label) {
    if (label && label[0]) {
        str_append(s, " --label ");
        append_shell_word(s, label);
    }
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

char *script_generate_start_herdr_ex(const Project *p, const ScriptOptions *opts) {
    Str s = str_with_capacity(4096);
    int do_attach = effective_attach(p, opts);
    int use_pre_window = effective_use_pre_window(opts);
    Project pstorage;
    char *abs_root_alloc = NULL;
    p = script_project_abs_root(p, &pstorage, &abs_root_alloc);

    str_append(&s, "#!/usr/bin/env bash\n");
    str_append(&s, "# shellcheck disable=SC2016,SC2034\n");
    str_append(&s, "set -euo pipefail\n\n");
    str_append(&s, "herdr_cmd=${MUX_HERDR_COMMAND:-herdr}\n\n");
    str_append(&s, "mux_herdr_server_running() {\n");
    str_append(&s, "  case $(\"$herdr_cmd\" status server 2>/dev/null || true) in\n");
    str_append(&s, "    *'status: running'*) return 0 ;;\n");
    str_append(&s, "    *) return 1 ;;\n");
    str_append(&s, "  esac\n");
    str_append(&s, "}\n\n");
    str_append(&s, "mux_herdr_ensure_server() {\n");
    str_append(&s, "  if mux_herdr_server_running; then\n");
    str_append(&s, "    return 0\n");
    str_append(&s, "  fi\n");
    str_append(&s, "  \"$herdr_cmd\" server >/dev/null 2>&1 &\n");
    str_append(&s, "  for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 "
                   "21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 "
                   "41 42 43 44 45 46 47 48 49 50; do\n");
    str_append(&s, "    if mux_herdr_server_running; then\n");
    str_append(&s, "      return 0\n");
    str_append(&s, "    fi\n");
    str_append(&s, "    sleep 0.1\n");
    str_append(&s, "  done\n");
    str_append(&s, "  echo \"mux: failed to start Herdr server\" >&2\n");
    str_append(&s, "  exit 1\n");
    str_append(&s, "}\n\n");
    str_append(&s, "mux_herdr_need_python() {\n");
    str_append(&s, "  if ! command -v python3 >/dev/null 2>&1; then\n");
    str_append(&s, "    echo \"mux: herdr backend requires python3 for Herdr JSON parsing\" >&2\n");
    str_append(&s, "    exit 1\n");
    str_append(&s, "  fi\n");
    str_append(&s, "}\n\n");
    str_append(&s, "mux_herdr_json_values() {\n");
    str_append(&s, "  python3 -c 'import json, sys\n");
    str_append(&s, "keys = sys.argv[1:]\n");
    str_append(&s, "try:\n");
    str_append(&s, "    doc = json.load(sys.stdin)\n");
    str_append(&s, "except Exception as exc:\n");
    str_append(&s, "    print(f\"mux: herdr returned invalid JSON while reading "
                   "{\",\".join(keys)}: {exc}\", file=sys.stderr)\n");
    str_append(&s, "    sys.exit(1)\n");
    str_append(&s, "def walk(value, target):\n");
    str_append(&s, "    if isinstance(value, dict):\n");
    str_append(&s, "        if target in value:\n");
    str_append(&s, "            return value[target]\n");
    str_append(&s, "        for child in value.values():\n");
    str_append(&s, "            found = walk(child, target)\n");
    str_append(&s, "            if found is not None:\n");
    str_append(&s, "                return found\n");
    str_append(&s, "    elif isinstance(value, list):\n");
    str_append(&s, "        for child in value:\n");
    str_append(&s, "            found = walk(child, target)\n");
    str_append(&s, "            if found is not None:\n");
    str_append(&s, "                return found\n");
    str_append(&s, "    return None\n");
    str_append(&s, "out = []\n");
    str_append(&s, "for key in keys:\n");
    str_append(&s, "    value = walk(doc, key)\n");
    str_append(&s, "    if value is None:\n");
    str_append(&s, "        print(f\"mux: herdr JSON response did not include {key}\", "
                   "file=sys.stderr)\n");
    str_append(&s, "        sys.exit(1)\n");
    str_append(&s, "    out.append(str(value))\n");
    str_append(&s, "print(\"\\t\".join(out))\n");
    str_append(&s, "' \"$@\"\n");
    str_append(&s, "}\n\n");
    str_append(&s, "mux_herdr_workspace_by_label() {\n");
    str_append(&s, "  \"$herdr_cmd\" workspace list | python3 -c 'import json, sys\n");
    str_append(&s, "label = sys.argv[1]\n");
    str_append(&s, "try:\n");
    str_append(&s, "    doc = json.load(sys.stdin)\n");
    str_append(&s, "except Exception as exc:\n");
    str_append(&s, "    print(f\"mux: herdr returned invalid JSON while reading workspace list: "
                   "{exc}\", file=sys.stderr)\n");
    str_append(&s, "    sys.exit(1)\n");
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
    str_append(&s, "mux_herdr_ensure_server\n\n");

    append_cd_project_root(&s, p, 1);
    append_herdr_hook(&s, p->on_project_start);

    str_append(&s, "workspace_id=$(mux_herdr_workspace_by_label ");
    append_shell_word(&s, p->name);
    str_append(&s, ")\n");
    str_append(&s, "if [ -n \"$workspace_id\" ]; then\n");
    str_append(&s, "  \"$herdr_cmd\" workspace focus \"$workspace_id\" >/dev/null\n");
    append_herdr_hook(&s, p->on_project_restart);
    if (do_attach) {
        str_append(&s, "  mux_herdr_attach\n");
    }
    append_herdr_hook(&s, p->on_project_exit);
    str_append(&s, "  exit 0\n");
    str_append(&s, "fi\n\n");

    append_herdr_hook(&s, p->on_project_first_start);

    const char *first_win_name =
        (p->window_count > 0 && p->windows[0].name && p->windows[0].name[0]) ? p->windows[0].name
                                                                             : "main";
    Str first_root = str_new();
    int has_first_root =
        resolve_window_root(p, (p->window_count > 0) ? &p->windows[0] : NULL, &first_root);

    str_append(&s, "workspace_json=$(\"$herdr_cmd\" workspace create --focus");
    if (has_first_root) {
        append_herdr_cwd_arg(&s, str_cstr(&first_root));
    }
    append_herdr_label_arg(&s, p->name);
    str_append(&s, ")\n");
    str_append(&s, "workspace_fields=$(printf '%s\\n' \"$workspace_json\" | "
                   "mux_herdr_json_values workspace_id tab_id pane_id)\n");
    str_append(&s, "IFS=$'\\t' read -r workspace_id tab_0 pane_0_0 <<< \"$workspace_fields\"\n");
    str_append(&s, "\"$herdr_cmd\" tab rename \"$tab_0\" ");
    append_shell_word(&s, first_win_name);
    str_append(&s, " >/dev/null\n\n");
    str_free(&first_root);

    int startup_wi = resolve_startup_window_index(p);
    Str wr = str_new();
    for (int wi = 0; wi < p->window_count; wi++) {
        const Window *w = &p->windows[wi];
        int has_root = resolve_window_root(p, w, &wr);
        const char *wlabel = (w->name && w->name[0]) ? w->name : "main";
        int win_focus = resolve_focused_pane_index(w);
        if (wi == startup_wi &&
            (p->startup_pane >= 0 || p->startup_pane == MUX_STARTUP_PANE_INVALID)) {
            int sp = (p->startup_pane == MUX_STARTUP_PANE_INVALID) ? 0 : p->startup_pane;
            win_focus = (sp < w->pane_count) ? sp : 0;
        }

        str_appendf(&s, "# Window/tab: %s\n", wlabel);
        if (wi > 0) {
            str_appendf(
                &s,
                "tab_json_%d=$(\"$herdr_cmd\" tab create --workspace \"$workspace_id\" --no-focus",
                wi);
            if (has_root) {
                append_herdr_cwd_arg(&s, str_cstr(&wr));
            }
            append_herdr_label_arg(&s, wlabel);
            str_append(&s, ")\n");
            str_appendf(&s,
                        "tab_fields_%d=$(printf '%%s\\n' \"$tab_json_%d\" | "
                        "mux_herdr_json_values tab_id pane_id)\n"
                        "IFS=$'\\t' read -r tab_%d pane_%d_0 <<< \"$tab_fields_%d\"\n",
                        wi, wi, wi, wi, wi);
        }

        const char *direction = herdr_split_direction(w);
        for (int pi = 0; pi < w->pane_count; pi++) {
            char pane_var[64];
            snprintf(pane_var, sizeof(pane_var), "pane_%d_%d", wi, pi);
            if (pi > 0) {
                char prev_pane_var[64];
                snprintf(prev_pane_var, sizeof(prev_pane_var), "pane_%d_%d", wi, pi - 1);
                double split_ratio = 1.0 / (double)(w->pane_count - pi + 1);
                str_appendf(&s,
                            "split_json_%d_%d=$(\"$herdr_cmd\" pane split \"$%s\" --direction %s "
                            "--ratio %.6f %s",
                            wi, pi, prev_pane_var, direction, split_ratio,
                            (pi == win_focus) ? "--focus" : "--no-focus");
                if (has_root) {
                    append_herdr_cwd_arg(&s, str_cstr(&wr));
                }
                str_append(&s, ")\n");
                str_appendf(&s,
                            "%s=$(printf '%%s\\n' \"$split_json_%d_%d\" | "
                            "mux_herdr_json_values pane_id)\n",
                            pane_var, wi, pi);
            }

            const Pane *pn = &w->panes[pi];
            if (pn->title && pn->title[0]) {
                str_append(&s, "\"$herdr_cmd\" pane rename \"$");
                str_append(&s, pane_var);
                str_append(&s, "\" ");
                append_shell_word(&s, pn->title);
                str_append(&s, " >/dev/null\n");
            }
            if (use_pre_window && p->pre_window && p->pre_window[0]) {
                append_herdr_send_command(&s, pane_var, p->pre_window);
            }
            if (w->pre && w->pre[0]) {
                append_herdr_send_command(&s, pane_var, w->pre);
            }
            for (int ci = 0; ci < pn->command_count; ci++) {
                append_herdr_send_command(&s, pane_var, pn->commands[ci]);
            }
        }

        str_append(&s, "\n");
    }
    str_free(&wr);

    str_append(&s, "if [ -n \"${workspace_id:-}\" ]; then\n");
    str_append(&s, "  \"$herdr_cmd\" workspace focus \"$workspace_id\" >/dev/null\n");
    str_append(&s, "fi\n");

    if (p->window_count > 0 && p->startup_window && p->startup_window[0]) {
        str_appendf(&s, "\"$herdr_cmd\" tab focus \"$tab_%d\" >/dev/null\n", startup_wi);
    }

    if (do_attach) {
        str_append(&s, "mux_herdr_attach\n");
    }

    append_herdr_hook(&s, p->on_project_exit);

    char *result = strdup(str_cstr(&s));
    str_free(&s);
    free(abs_root_alloc);
    return result;
}

char *script_generate_start_herdr(const Project *p) {
    return script_generate_start_herdr_ex(p, NULL);
}

char *script_generate_stop_herdr(const Project *p) {
    Str s = str_with_capacity(1024);
    Project pstorage;
    char *abs_root_alloc = NULL;
    p = script_project_abs_root(p, &pstorage, &abs_root_alloc);

    str_append(&s, "#!/usr/bin/env bash\n");
    str_append(&s, "set -euo pipefail\n\n");
    str_append(&s, "herdr_cmd=${MUX_HERDR_COMMAND:-herdr}\n\n");
    append_cd_project_root(&s, p, 1);
    append_herdr_hook(&s, p->on_project_stop);
    str_append(&s, "if ! command -v python3 >/dev/null 2>&1; then\n");
    str_append(&s, "  echo \"mux: herdr backend requires python3 for Herdr JSON parsing\" >&2\n");
    str_append(&s, "  exit 1\n");
    str_append(&s, "fi\n");
    str_append(&s, "\"$herdr_cmd\" workspace list | python3 -c 'import json, sys\n");
    str_append(&s, "label = sys.argv[1]\n");
    str_append(&s, "try:\n");
    str_append(&s, "    doc = json.load(sys.stdin)\n");
    str_append(&s, "except Exception as exc:\n");
    str_append(&s, "    print(f\"mux: herdr returned invalid JSON while reading workspace list: "
                   "{exc}\", file=sys.stderr)\n");
    str_append(&s, "    sys.exit(1)\n");
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
    free(abs_root_alloc);
    return result;
}

static void append_herdr_app_prelude(Str *s) {
    str_append(s, "#!/usr/bin/env bash\n"
                  "# shellcheck disable=SC1010,SC2016,SC2034\n"
                  "set -euo pipefail\n\n"
                  "herdr_app_cmd=${MUX_HERDR_APP_COMMAND:-herdr-app}\n"
                  "hc_cmd=${MUX_HC_COMMAND:-hc}\n"
                  "for cmd in \"$herdr_app_cmd\" python3; do\n"
                  "  if ! command -v \"$cmd\" >/dev/null 2>&1; then\n"
                  "    echo \"mux: herdr-app backend requires $cmd\" >&2\n"
                  "    exit 1\n"
                  "  fi\n"
                  "done\n"
                  "if ! app_json=$(\"$herdr_app_cmd\" ls); then\n"
                  "  echo \"mux: herdr-app backend requires an open Herdr app window\" >&2\n"
                  "  exit 1\n"
                  "fi\n\n"
                  "mux_app_json() {\n"
                  "  python3 -c 'import json, sys\n"
                  "mode = sys.argv[1]\n"
                  "keys = sys.argv[2:]\n"
                  "try:\n"
                  "    doc = json.load(sys.stdin)\n"
                  "    if mode == \"space\":\n"
                  "        key = keys[0]\n"
                  "        matches = [s[\"id\"] for s in doc[\"spaces\"]\n"
                  "                   if not s.get(\"machine\") and s[\"name\"] == key]\n"
                  "        if len(matches) > 1:\n"
                  "            raise ValueError(\"more than one local space has that name\")\n"
                  "        value = matches[0] if matches else \"\"\n"
                  "        print(value)\n"
                  "    elif mode == \"pane\":\n"
                  "        key = keys[0]\n"
                  "        value = next(p[\"pane\"] for s in doc[\"spaces\"]\n"
                  "                     if not s.get(\"machine\") for t in s[\"tabs\"]\n"
                  "                     for p in t[\"terminals\"] if p[\"terminal\"] == key)\n"
                  "        if not isinstance(value, str) or not value:\n"
                  "            raise ValueError(\"missing identifier\")\n"
                  "        print(value)\n"
                  "    else:\n"
                  "        out = []\n"
                  "        for key in keys:\n"
                  "            value = doc[key]\n"
                  "            if not isinstance(value, str) or not value:\n"
                  "                raise ValueError(\"missing identifier\")\n"
                  "            out.append(value)\n"
                  "        print(\"\\t\".join(out))\n"
                  "except (ValueError, KeyError, TypeError, StopIteration, IndexError) as exc:\n"
                  "    print(f\"mux: invalid herdr-app response for {mode} {keys}: {exc}\", "
                  "file=sys.stderr)\n"
                  "    sys.exit(1)\n"
                  "' \"$@\"\n"
                  "}\n\n");
}

static void append_herdr_app_folder(Str *s, const char *root) {
    str_append(s, " --folder ");
    if (root && root[0]) {
        append_shell_path(s, root);
    } else {
        str_append(s, "\"$PWD\"");
    }
}

static void append_herdr_app_commands(Str *s, const Project *p, const Window *w, int wi, int pi,
                                      int use_pre_window) {
    Str commands = str_new();
    if (use_pre_window && p->pre_window && p->pre_window[0]) {
        str_appendf(&commands, "%s\n", p->pre_window);
    }
    if (w->pre && w->pre[0]) {
        str_appendf(&commands, "%s\n", w->pre);
    }
    for (int ci = 0; ci < w->panes[pi].command_count; ci++) {
        if (ci > 0) str_append_char(&commands, '\n');
        str_append(&commands, w->panes[pi].commands[ci]);
    }
    /* The desktop CLI can inherit a split's cwd from a different focused tab. */
    Str root = str_new();
    int has_root = resolve_window_root(p, w, &root);
    str_appendf(s, "mux_app_send \"$terminal_%d_%d\" ", wi, pi);
    if (has_root) {
        append_shell_path(s, str_cstr(&root));
    } else {
        str_append(s, "\"$PWD\"");
    }
    str_append_char(s, ' ');
    append_shell_word(s, str_cstr(&commands));
    str_append_char(s, '\n');
    str_free(&root);
    str_free(&commands);
}

char *script_generate_start_herdr_app_ex(const Project *p, const ScriptOptions *opts) {
    Str s = str_with_capacity(4096);
    int use_pre_window = effective_use_pre_window(opts);
    Project pstorage;
    char *abs_root_alloc = NULL;
    p = script_project_abs_root(p, &pstorage, &abs_root_alloc);

    append_herdr_app_prelude(&s);
    str_append(&s, "if ! command -v \"$hc_cmd\" >/dev/null 2>&1; then\n"
                   "  echo \"mux: herdr-app backend requires $hc_cmd\" >&2\n"
                   "  exit 1\n"
                   "fi\n\n"
                   "mux_app_send() {\n"
                   "  local text\n"
                   "  printf -v text 'cd -- %q\\n%s' \"$2\" \"$3\"\n"
                   "  \"$hc_cmd\" send -- \"$1\" \"$text\" >/dev/null\n"
                   "}\n\n");

    append_cd_project_root(&s, p, 1);
    append_herdr_hook(&s, p->on_project_start);

    str_append(&s, "space_id=$(printf '%s\\n' \"$app_json\" | mux_app_json space ");
    append_shell_word(&s, p->name);
    str_append(&s, ")\n"
                   "if [ -n \"$space_id\" ]; then\n"
                   "  \"$herdr_app_cmd\" do space.select --id \"$space_id\" >/dev/null\n");
    append_herdr_hook(&s, p->on_project_restart);
    append_herdr_hook(&s, p->on_project_exit);
    str_append(&s, "  exit 0\nfi\n\n");

    append_herdr_hook(&s, p->on_project_first_start);

    const char *first_name = (p->window_count > 0 && p->windows[0].name && p->windows[0].name[0])
                                 ? p->windows[0].name
                                 : "main";
    Str first_root = str_new();
    int has_first_root =
        resolve_window_root(p, (p->window_count > 0) ? &p->windows[0] : NULL, &first_root);

    str_append(&s, "created_json=$(\"$herdr_app_cmd\" do space.new --machine '' --name ");
    append_shell_word(&s, p->name);
    str_append(&s, " --tab ");
    append_shell_word(&s, first_name);
    append_herdr_app_folder(&s, has_first_root ? str_cstr(&first_root) : NULL);
    str_append(&s, ")\n");
    str_append(&s, "created_fields=$(printf '%s\\n' \"$created_json\" | "
                   "mux_app_json fields space tab terminal)\n");
    str_append(&s, "IFS=$'\\t' read -r space_id tab_0 terminal_0_0 <<< \"$created_fields\"\n");
    str_append(&s, "pane_0_0=$(\"$herdr_app_cmd\" ls | mux_app_json pane \"$terminal_0_0\")\n\n");
    str_free(&first_root);

    Str wr = str_new();
    for (int wi = 0; wi < p->window_count; wi++) {
        const Window *w = &p->windows[wi];
        const char *wlabel = (w->name && w->name[0]) ? w->name : "main";
        if (wi > 0) {
            int has_root = resolve_window_root(p, w, &wr);
            str_append(
                &s, "created_json=$(\"$herdr_app_cmd\" do tab.new --space \"$space_id\" --name ");
            append_shell_word(&s, wlabel);
            append_herdr_app_folder(&s, has_root ? str_cstr(&wr) : NULL);
            str_append(&s, ")\n");
            str_appendf(&s,
                        "created_fields=$(printf '%%s\\n' \"$created_json\" | "
                        "mux_app_json fields tab terminal)\n"
                        "IFS=$'\\t' read -r tab_%d terminal_%d_0 <<< \"$created_fields\"\n",
                        wi, wi);
            str_appendf(
                &s, "pane_%d_0=$(\"$herdr_app_cmd\" ls | mux_app_json pane \"$terminal_%d_0\")\n",
                wi, wi);
        }

        /* Split before sending commands so a pane's cd cannot change the next pane's root. */
        for (int pi = 1; pi < w->pane_count; pi++) {
            str_appendf(&s,
                        "created_json=$(\"$herdr_app_cmd\" do pane.split --pane \"$pane_%d_%d\" "
                        "--direction %s)\n",
                        wi, pi - 1, herdr_split_direction(w));
            str_appendf(&s,
                        "created_fields=$(printf '%%s\\n' \"$created_json\" | "
                        "mux_app_json fields pane terminal)\n"
                        "IFS=$'\\t' read -r pane_%d_%d terminal_%d_%d <<< \"$created_fields\"\n",
                        wi, pi, wi, pi);
        }
        for (int pi = 0; pi < w->pane_count; pi++) {
            append_herdr_app_commands(&s, p, w, wi, pi, use_pre_window);
        }
        int focus = resolve_focused_pane_index(w);
        if (w->pane_count > 1 || (w->focused_pane && w->focused_pane[0])) {
            str_appendf(&s, "\"$herdr_app_cmd\" do pane.focus --pane \"$pane_%d_%d\" >/dev/null\n",
                        wi, focus);
        }
        str_append_char(&s, '\n');
    }
    str_free(&wr);

    int startup_window = resolve_startup_window_index(p);
    int startup_pane =
        p->window_count > 0 ? resolve_focused_pane_index(&p->windows[startup_window]) : 0;
    if (p->window_count > 0 &&
        (p->startup_pane >= 0 || p->startup_pane == MUX_STARTUP_PANE_INVALID)) {
        int sp = (p->startup_pane == MUX_STARTUP_PANE_INVALID) ? 0 : p->startup_pane;
        startup_pane = (sp < p->windows[startup_window].pane_count) ? sp : 0;
    }
    str_appendf(&s, "\"$herdr_app_cmd\" do tab.select --id \"$tab_%d\" >/dev/null\n",
                startup_window);
    str_appendf(&s, "\"$herdr_app_cmd\" do pane.focus --pane \"$pane_%d_%d\" >/dev/null\n",
                startup_window, startup_pane);
    append_herdr_hook(&s, p->on_project_exit);

    char *result = strdup(str_cstr(&s));
    str_free(&s);
    free(abs_root_alloc);
    return result;
}

char *script_generate_start_herdr_app(const Project *p) {
    return script_generate_start_herdr_app_ex(p, NULL);
}

char *script_generate_stop_herdr_app(const Project *p) {
    Str s = str_with_capacity(2048);
    Project pstorage;
    char *abs_root_alloc = NULL;
    p = script_project_abs_root(p, &pstorage, &abs_root_alloc);
    append_herdr_app_prelude(&s);
    append_cd_project_root(&s, p, 1);
    str_append(&s, "space_id=$(printf '%s\\n' \"$app_json\" | mux_app_json space ");
    append_shell_word(&s, p->name);
    str_append(&s, ")\n");
    append_herdr_hook(&s, p->on_project_stop);
    str_append(
        &s, "if [ -n \"$space_id\" ]; then\n"
            "  \"$herdr_app_cmd\" do space.close --id \"$space_id\" --confirmed true >/dev/null\n"
            "fi\n");
    char *result = strdup(str_cstr(&s));
    str_free(&s);
    free(abs_root_alloc);
    return result;
}
