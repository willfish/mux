#include "path.h"

#include <dirent.h>
#include <errno.h>
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

static int add_unique_dir(Arena *a, char **dirs, int count, int max_dirs, const char *candidate) {
    if (!candidate || !candidate[0] || count >= max_dirs) return count;
    for (int i = 0; i < count; i++) {
        if (strcmp(dirs[i], candidate) == 0) return count;
    }
    dirs[count] = arena_strdup(a, candidate);
    return count + 1;
}

/* Collect candidate config directories in precedence order (deduplicated).
 * When $TMUXINATOR_CONFIG is explicitly set, only that directory is used. */
static int collect_config_dirs(Arena *a, char **dirs, int max_dirs) {
    int count = 0;

    const char *tc = getenv("TMUXINATOR_CONFIG");
    if (tc && tc[0]) {
        return add_unique_dir(a, dirs, count, max_dirs, tc);
    }

    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");

    if (xdg && xdg[0]) {
        Str buf = str_new();
        str_appendf(&buf, "%s/tmuxinator", xdg);
        count = add_unique_dir(a, dirs, count, max_dirs, str_cstr(&buf));
        str_free(&buf);
    }

    if (home && home[0]) {
        Str buf = str_new();
        str_appendf(&buf, "%s/.config/tmuxinator", home);
        count = add_unique_dir(a, dirs, count, max_dirs, str_cstr(&buf));
        str_clear(&buf);
        str_appendf(&buf, "%s/.tmuxinator", home);
        count = add_unique_dir(a, dirs, count, max_dirs, str_cstr(&buf));
        str_free(&buf);
    }

    return count;
}

char *path_config_dir(Arena *a) {
    /* 1. $TMUXINATOR_CONFIG takes precedence even if not yet created */
    const char *tc = getenv("TMUXINATOR_CONFIG");
    if (tc && tc[0]) {
        return arena_strdup(a, tc);
    }

    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");

    /* 2. $XDG_CONFIG_HOME/tmuxinator */
    if (xdg && xdg[0]) {
        Str buf = str_new();
        str_appendf(&buf, "%s/tmuxinator", xdg);
        if (dir_exists(str_cstr(&buf))) {
            char *result = arena_strdup(a, str_cstr(&buf));
            str_free(&buf);
            return result;
        }
        str_free(&buf);
    }

    if (!home || !home[0]) {
        if (xdg && xdg[0]) {
            Str buf = str_new();
            str_appendf(&buf, "%s/tmuxinator", xdg);
            char *result = arena_strdup(a, str_cstr(&buf));
            str_free(&buf);
            return result;
        }
        fprintf(stderr, "mux: $HOME not set\n");
        return NULL;
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
        if (xdg && xdg[0]) {
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
    if (!name || !name[0]) return NULL;

    char *dirs[4];
    int dir_count = collect_config_dirs(a, dirs, 4);
    if (dir_count == 0) return NULL;

    static const char *exts[] = {".yml", ".yaml"};
    Str buf = str_new();

    for (int i = 0; i < dir_count; i++) {
        if (!dir_exists(dirs[i])) continue;
        for (size_t e = 0; e < sizeof(exts) / sizeof(exts[0]); e++) {
            str_clear(&buf);
            str_appendf(&buf, "%s/%s%s", dirs[i], name, exts[e]);
            if (file_exists(str_cstr(&buf))) {
                char *result = arena_strdup(a, str_cstr(&buf));
                str_free(&buf);
                return result;
            }
        }
    }

    str_free(&buf);
    return NULL;
}

char *path_find_local(Arena *a) {
    static const char *local_names[] = {".tmuxinator.yml", ".tmuxinator.yaml"};
    for (size_t i = 0; i < sizeof(local_names) / sizeof(local_names[0]); i++) {
        Str rel = str_new();
        str_appendf(&rel, "./%s", local_names[i]);
        if (file_exists(str_cstr(&rel))) {
            char cwd[4096];
            if (getcwd(cwd, sizeof(cwd))) {
                Str full = str_new();
                str_appendf(&full, "%s/%s", cwd, local_names[i]);
                char *result = arena_strdup(a, str_cstr(&full));
                str_free(&full);
                str_free(&rel);
                return result;
            }
            char *result = arena_strdup(a, str_cstr(&rel));
            str_free(&rel);
            return result;
        }
        str_free(&rel);
    }
    return NULL;
}

typedef struct {
    char **items;
    int count;
    int cap;
} ProjectList;

static void project_list_push(Arena *a, ProjectList *list, const char *name) {
    if (list->count >= list->cap) {
        int new_cap = list->cap == 0 ? 16 : list->cap * 2;
        char **new_items = arena_alloc(a, sizeof(char *) * (size_t)(new_cap + 1));
        if (list->count > 0) {
            memcpy(new_items, list->items, sizeof(char *) * (size_t)list->count);
        }
        list->items = new_items;
        list->cap = new_cap;
    }
    list->items[list->count++] = arena_strdup(a, name);
}

static size_t yaml_ext_len(const char *filename, size_t len) {
    if (len > 4 && strcmp(filename + len - 4, ".yml") == 0) return 4;
    if (len > 5 && strcmp(filename + len - 5, ".yaml") == 0) return 5;
    return 0;
}

static void list_projects_recursive(Arena *a, const char *base_dir, const char *prefix,
                                    ProjectList *list, int depth) {
    if (depth > 8) return; /* guard against symlink cycles */

    DIR *dir = opendir(base_dir);
    if (!dir) return;

    Str full = str_new();
    Str rel = str_new();

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue; /* skip ., .., and hidden files */

        int is_dir = 0;
        int is_reg = 0;

#ifdef DT_REG
        if (ent->d_type == DT_REG) {
            is_reg = 1;
        } else if (ent->d_type == DT_DIR) {
            is_dir = 1;
        } else
#endif
        {
            str_clear(&full);
            str_appendf(&full, "%s/%s", base_dir, ent->d_name);
            struct stat st;
            if (stat(str_cstr(&full), &st) != 0) continue;
            is_dir = S_ISDIR(st.st_mode);
            is_reg = S_ISREG(st.st_mode);
        }

        if (is_dir) {
            str_clear(&full);
            str_appendf(&full, "%s/%s", base_dir, ent->d_name);
            str_clear(&rel);
            if (prefix && prefix[0]) {
                str_appendf(&rel, "%s/%s", prefix, ent->d_name);
            } else {
                str_append(&rel, ent->d_name);
            }
            list_projects_recursive(a, str_cstr(&full), str_cstr(&rel), list, depth + 1);
        } else if (is_reg) {
            size_t len = strlen(ent->d_name);
            size_t ext = yaml_ext_len(ent->d_name, len);
            if (ext > 0) {
                str_clear(&rel);
                if (prefix && prefix[0]) {
                    str_appendf(&rel, "%s/", prefix);
                }
                str_appendn(&rel, ent->d_name, len - ext);
                project_list_push(a, list, str_cstr(&rel));
            }
        }
    }

    str_free(&full);
    str_free(&rel);
    closedir(dir);
}

static int cmp_str_ptr(const void *lhs, const void *rhs) {
    const char *a = *(const char *const *)lhs;
    const char *b = *(const char *const *)rhs;
    return strcmp(a, b);
}

static char **finish_project_list(Arena *a, ProjectList *list, int *count) {
    if (list->count == 0) {
        char **empty = arena_alloc(a, sizeof(char *));
        empty[0] = NULL;
        *count = 0;
        return empty;
    }

    qsort(list->items, (size_t)list->count, sizeof(char *), cmp_str_ptr);

    /* Deduplicate sorted entries */
    int unique = 0;
    for (int i = 0; i < list->count; i++) {
        if (unique == 0 || strcmp(list->items[i], list->items[unique - 1]) != 0) {
            list->items[unique++] = list->items[i];
        }
    }
    list->items[unique] = NULL;
    *count = unique;
    return list->items;
}

/* List project names found under a single config directory (recursively,
 * including nested "team/beta" style names). Returns NULL only when the
 * directory itself is missing; an existing but empty directory yields an
 * empty list. */
char **path_list_projects_in(Arena *a, const char *base_dir, int *count) {
    *count = 0;
    if (!base_dir || !base_dir[0] || !dir_exists(base_dir)) return NULL;

    ProjectList list = {0};
    list_projects_recursive(a, base_dir, NULL, &list, 0);
    return finish_project_list(a, &list, count);
}

char **path_list_projects(Arena *a, int *count) {
    *count = 0;

    char *dirs[4];
    int dir_count = collect_config_dirs(a, dirs, 4);
    if (dir_count == 0) return NULL;

    ProjectList list = {0};
    int found_any_dir = 0;
    for (int i = 0; i < dir_count; i++) {
        if (!dir_exists(dirs[i])) continue;
        found_any_dir = 1;
        list_projects_recursive(a, dirs[i], NULL, &list, 0);
    }

    if (!found_any_dir) return NULL;

    return finish_project_list(a, &list, count);
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

int path_mkdir_p(const char *path) {
    if (!path || !path[0]) return -1;

    char *copy = strdup(path);
    if (!copy) return -1;

    size_t len = strlen(copy);
    while (len > 1 && copy[len - 1] == '/') {
        copy[--len] = '\0';
    }

    for (char *p = copy + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
                free(copy);
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
        free(copy);
        return -1;
    }

    free(copy);
    return 0;
}
