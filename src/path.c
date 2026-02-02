#include "path.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "str.h"

static int dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

char *path_config_dir(Arena *a) {
    /* 1. $TMUXINATOR_CONFIG */
    const char *tc = getenv("TMUXINATOR_CONFIG");
    if (tc && dir_exists(tc)) {
        return arena_strdup(a, tc);
    }

    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "mux: $HOME not set\n");
        return NULL;
    }

    /* 2. $XDG_CONFIG_HOME/tmuxinator */
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg) {
        Str buf = str_new();
        str_appendf(&buf, "%s/tmuxinator", xdg);
        if (dir_exists(str_cstr(&buf))) {
            char *result = arena_strdup(a, str_cstr(&buf));
            str_free(&buf);
            return result;
        }
        str_free(&buf);
    }

    /* 3. ~/.config/tmuxinator */
    {
        Str buf = str_new();
        str_appendf(&buf, "%s/.config/tmuxinator", home);
        if (dir_exists(str_cstr(&buf))) {
            char *result = arena_strdup(a, str_cstr(&buf));
            str_free(&buf);
            return result;
        }
        str_free(&buf);
    }

    /* 4. ~/.tmuxinator */
    {
        Str buf = str_new();
        str_appendf(&buf, "%s/.tmuxinator", home);
        if (dir_exists(str_cstr(&buf))) {
            char *result = arena_strdup(a, str_cstr(&buf));
            str_free(&buf);
            return result;
        }
        str_free(&buf);
    }

    /* Default: XDG default */
    {
        Str buf = str_new();
        if (xdg) {
            str_appendf(&buf, "%s/tmuxinator", xdg);
        } else {
            str_appendf(&buf, "%s/.config/tmuxinator", home);
        }
        char *result = arena_strdup(a, str_cstr(&buf));
        str_free(&buf);
        return result;
    }
}

char *path_find_project(Arena *a, const char *name) {
    if (!name) return NULL;

    char *config_dir = path_config_dir(a);
    if (!config_dir) return NULL;

    Str buf = str_new();
    str_appendf(&buf, "%s/%s.yml", config_dir, name);
    if (file_exists(str_cstr(&buf))) {
        char *result = arena_strdup(a, str_cstr(&buf));
        str_free(&buf);
        return result;
    }
    str_free(&buf);

    return NULL;
}

char *path_find_local(Arena *a) {
    if (file_exists("./.tmuxinator.yml")) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            Str buf = str_new();
            str_appendf(&buf, "%s/.tmuxinator.yml", cwd);
            char *result = arena_strdup(a, str_cstr(&buf));
            str_free(&buf);
            return result;
        }
        return arena_strdup(a, "./.tmuxinator.yml");
    }
    return NULL;
}

char **path_list_projects(Arena *a, int *count) {
    *count = 0;
    char *config_dir = path_config_dir(a);
    if (!config_dir) return NULL;

    DIR *dir = opendir(config_dir);
    if (!dir) return NULL;

    /* First pass: count .yml files */
    int capacity = 16;
    char **names = arena_alloc(a, sizeof(char *) * (size_t)(capacity + 1));
    int n = 0;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, ".yml") == 0) {
            if (n >= capacity) {
                capacity *= 2;
                char **new_names = arena_alloc(a, sizeof(char *) * (size_t)(capacity + 1));
                memcpy(new_names, names, sizeof(char *) * (size_t)n);
                names = new_names;
            }
            names[n] = arena_strndup(a, ent->d_name, len - 4);
            n++;
        }
    }
    closedir(dir);

    names[n] = NULL;
    *count = n;
    return names;
}

char *path_project_file(Arena *a, const char *name) {
    char *config_dir = path_config_dir(a);
    if (!config_dir) return NULL;

    Str buf = str_new();
    str_appendf(&buf, "%s/%s.yml", config_dir, name);
    char *result = arena_strdup(a, str_cstr(&buf));
    str_free(&buf);
    return result;
}
