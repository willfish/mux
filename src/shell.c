#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "str.h"

char *shell_escape(Arena *a, const char *s) {
    if (!s) return arena_strdup(a, "''");

    /* Empty string */
    if (s[0] == '\0') {
        return arena_strdup(a, "''");
    }

    Str buf = str_new();
    str_append_char(&buf, '\'');
    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            str_append(&buf, "'\"'\"'");
        } else {
            str_append_char(&buf, *p);
        }
    }
    str_append_char(&buf, '\'');

    char *result = arena_strdup(a, str_cstr(&buf));
    str_free(&buf);
    return result;
}

char *path_expand(Arena *a, const char *path) {
    if (!path) return NULL;

    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) return arena_strdup(a, path);

        Str buf = str_new();
        str_append(&buf, home);
        str_append(&buf, path + 1);
        char *result = arena_strdup(a, str_cstr(&buf));
        str_free(&buf);
        return result;
    }
    return arena_strdup(a, path);
}

int shell_exec(const char *cmd) {
    int status = system(cmd);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

int shell_exec_bash(const char *script) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("mux: fork");
        return -1;
    }
    if (pid == 0) {
        execl("/bin/bash", "bash", "-c", script, (char *)NULL);
        perror("mux: exec");
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}
