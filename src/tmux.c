#include "tmux.h"

#include "shell.h"
#include "str.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *tmux_sanitize_session_name(Arena *a, const char *session_name) {
    if (!session_name) return arena_strdup(a, "");
    char *copy = arena_strdup(a, session_name);
    for (char *p = copy; *p; p++) {
        if (*p == '.' || *p == ':') {
            *p = '_';
        }
    }
    return copy;
}

char *tmux_has_session_command(Arena *a, const char *session_name) {
    char *sanitized = tmux_sanitize_session_name(a, session_name);
    Str target = str_new();
    str_append_char(&target, '=');
    str_append(&target, sanitized);

    Str buf = str_new();
    str_append(&buf, "tmux has-session -t ");
    str_append(&buf, shell_escape(a, str_cstr(&target)));
    str_append(&buf, " 2>/dev/null");

    char *result = arena_strdup(a, str_cstr(&buf));
    str_free(&target);
    str_free(&buf);
    return result;
}

int tmux_has_session(Arena *a, const char *session_name) {
    char *cmd = tmux_has_session_command(a, session_name);
    return shell_exec(cmd) == 0;
}

int tmux_project_has_session(Arena *a, const Project *p) {
    if (!p || !p->name || !p->name[0]) return 0;

    char *sanitized = tmux_sanitize_session_name(a, p->name);
    Str target = str_new();
    str_append_char(&target, '=');
    str_append(&target, sanitized);

    Str buf = str_new();
    const char *cmd = (p->tmux_command && p->tmux_command[0]) ? p->tmux_command : "tmux";
    str_append(&buf, shell_escape(a, cmd));
    if (p->socket_name && p->socket_name[0]) {
        str_append(&buf, " -L ");
        str_append(&buf, shell_escape(a, p->socket_name));
    }
    if (p->socket_path && p->socket_path[0]) {
        str_append(&buf, " -S ");
        str_append(&buf, shell_escape(a, p->socket_path));
    }
    if (p->tmux_options && p->tmux_options[0]) {
        str_appendf(&buf, " %s", p->tmux_options);
    }
    str_append(&buf, " has-session -t ");
    str_append(&buf, shell_escape(a, str_cstr(&target)));
    str_append(&buf, " 2>/dev/null");

    int exists = shell_exec(str_cstr(&buf)) == 0;
    str_free(&target);
    str_free(&buf);
    return exists;
}

static int cmp_session_name(const void *lhs, const void *rhs) {
    const char *a = *(const char *const *)lhs;
    const char *b = *(const char *const *)rhs;
    return strcmp(a, b);
}

char **tmux_parse_session_names(Arena *a, const char *output, int *count) {
    *count = 0;
    if (!output) {
        char **empty = arena_alloc(a, sizeof(char *));
        empty[0] = NULL;
        return empty;
    }

    int lines = 0;
    const char *line = output;
    while (*line) {
        const char *line_end = strchr(line, '\n');
        size_t len = line_end ? (size_t)(line_end - line) : strlen(line);
        if (len > 0) lines++;
        if (!line_end) break;
        line = line_end + 1;
    }

    char **sessions = arena_alloc(a, sizeof(char *) * (size_t)(lines + 1));
    int n = 0;
    line = output;
    while (*line) {
        const char *line_end = strchr(line, '\n');
        size_t len = line_end ? (size_t)(line_end - line) : strlen(line);
        if (len > 0) {
            sessions[n++] = arena_strndup(a, line, len);
        }
        if (!line_end) break;
        line = line_end + 1;
    }
    if (n > 1) {
        qsort(sessions, (size_t)n, sizeof(char *), cmp_session_name);
    }
    sessions[n] = NULL;
    *count = n;
    return sessions;
}

int tmux_session_names_contain(char **session_names, int count, const char *session_name) {
    if (!session_names || !session_name || count <= 0) return 0;

    int lo = 0;
    int hi = count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (!session_names[mid]) return 0;
        int cmp = strcmp(session_names[mid], session_name);
        if (cmp == 0) return 1;
        if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return 0;
}

char **tmux_list_sessions(Arena *a, int *count) {
    *count = 0;
    FILE *pipe = popen("tmux list-sessions -F '#S' 2>/dev/null", "r");
    if (!pipe) {
        return tmux_parse_session_names(a, "", count);
    }

    Str output = str_new();
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        str_append(&output, buf);
    }
    pclose(pipe);

    char **sessions = tmux_parse_session_names(a, str_cstr(&output), count);
    str_free(&output);
    return sessions;
}
