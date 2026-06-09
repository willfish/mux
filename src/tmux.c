#include "tmux.h"

#include "shell.h"
#include "str.h"

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
