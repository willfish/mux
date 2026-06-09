#include "tmux.h"

#include "shell.h"
#include "str.h"

#include <stdio.h>
#include <string.h>

char *tmux_has_session_command(Arena *a, const char *session_name) {
    Str buf = str_new();
    str_append(&buf, "tmux has-session -t ");
    str_append(&buf, shell_escape(a, session_name));
    str_append(&buf, " 2>/dev/null");
    char *result = arena_strdup(a, str_cstr(&buf));
    str_free(&buf);
    return result;
}

int tmux_has_session(Arena *a, const char *session_name) {
    char *cmd = tmux_has_session_command(a, session_name);
    return shell_exec(cmd) == 0;
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
        if (len > 0)
            lines++;
        if (!line_end)
            break;
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
        if (!line_end)
            break;
        line = line_end + 1;
    }
    sessions[n] = NULL;
    *count = n;
    return sessions;
}

int tmux_session_names_contain(char **session_names, int count, const char *session_name) {
    if (!session_names || !session_name)
        return 0;

    for (int i = 0; i < count; i++) {
        if (session_names[i] && strcmp(session_names[i], session_name) == 0) {
            return 1;
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
