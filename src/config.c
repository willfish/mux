#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <yaml.h>

#include "str.h"
#include "template.h"

/* Map deprecated field names to canonical names */
static const char *config_canonical_key(const char *key) {
    if (strcmp(key, "project_name") == 0) return "name";
    if (strcmp(key, "project_root") == 0) return "root";
    if (strcmp(key, "cli_args") == 0) return "tmux_options";
    if (strcmp(key, "tabs") == 0) return "windows";
    if (strcmp(key, "rbenv") == 0) return NULL; /* ignored */
    if (strcmp(key, "rvm") == 0) return NULL;   /* ignored */
    if (strcmp(key, "pre") == 0) return NULL;   /* deprecated, ignored */
    if (strcmp(key, "post") == 0) return NULL;  /* deprecated, ignored */
    return key;
}

static int is_yaml_null(const yaml_node_t *node) {
    if (!node || node->type != YAML_SCALAR_NODE) return 0;
    const char *val = (const char *)node->data.scalar.value;
    if (!val || val[0] == '\0') return 1;
    if (node->data.scalar.style == YAML_PLAIN_SCALAR_STYLE) {
        if (strcmp(val, "~") == 0 || strcmp(val, "null") == 0 || strcmp(val, "Null") == 0 ||
            strcmp(val, "NULL") == 0) {
            return 1;
        }
    }
    return 0;
}

/* Parse YAML 1.1 boolean values (case-insensitive). Returns 1 if recognised, 0 otherwise. */
static int try_parse_yaml_bool(const yaml_node_t *node, bool *out) {
    if (!node || node->type != YAML_SCALAR_NODE) return 0;
    const char *sv = (const char *)node->data.scalar.value;
    if (!sv) return 0;
    if (strcasecmp(sv, "true") == 0 || strcasecmp(sv, "yes") == 0 || strcasecmp(sv, "on") == 0 ||
        strcmp(sv, "1") == 0) {
        *out = true;
        return 1;
    }
    if (strcasecmp(sv, "false") == 0 || strcasecmp(sv, "no") == 0 || strcasecmp(sv, "off") == 0 ||
        strcmp(sv, "0") == 0) {
        *out = false;
        return 1;
    }
    return 0;
}

static int parse_nonnegative_int(const char *sv, int *out) {
    if (!sv || !sv[0]) return 0;
    while (isspace((unsigned char)*sv)) sv++;
    if (*sv < '0' || *sv > '9') return 0;
    errno = 0;
    char *end = NULL;
    long val = strtol(sv, &end, 10);
    if (errno != 0 || end == sv) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0' || val < 0 || val > INT_MAX) return 0;
    *out = (int)val;
    return 1;
}

/* Join a YAML scalar or sequence of strings with sep, skipping null/empty entries. */
static char *join_yaml_sequence(Arena *a, yaml_document_t *doc, yaml_node_t *node,
                                const char *sep) {
    if (node->type == YAML_SCALAR_NODE) {
        if (is_yaml_null(node)) return NULL;
        return arena_strdup(a, (const char *)node->data.scalar.value);
    }
    if (node->type != YAML_SEQUENCE_NODE) return NULL;

    Str buf = str_new();
    int first = 1;
    for (yaml_node_item_t *item = node->data.sequence.items.start;
         item < node->data.sequence.items.top; item++) {
        yaml_node_t *val = yaml_document_get_node(doc, *item);
        if (val && val->type == YAML_SCALAR_NODE && !is_yaml_null(val)) {
            if (!first) str_append(&buf, sep);
            str_append(&buf, (const char *)val->data.scalar.value);
            first = 0;
        }
    }
    if (first) {
        str_free(&buf);
        return NULL;
    }
    char *result = arena_strdup(a, str_cstr(&buf));
    str_free(&buf);
    return result;
}

/* Collect a YAML sequence (or scalar) as an array of strings */
static char **collect_commands(Arena *a, yaml_document_t *doc, yaml_node_t *node, int *count) {
    *count = 0;

    if (node->type == YAML_SCALAR_NODE) {
        if (is_yaml_null(node)) return NULL;
        const char *val = (const char *)node->data.scalar.value;
        char **cmds = arena_alloc(a, sizeof(char *) * 2);
        cmds[0] = arena_strdup(a, val);
        cmds[1] = NULL;
        *count = 1;
        return cmds;
    }

    if (node->type != YAML_SEQUENCE_NODE) return NULL;

    int n = (int)(node->data.sequence.items.top - node->data.sequence.items.start);
    if (n == 0) return NULL;

    char **cmds = arena_alloc(a, sizeof(char *) * (size_t)(n + 1));
    int idx = 0;
    for (yaml_node_item_t *item = node->data.sequence.items.start;
         item < node->data.sequence.items.top; item++) {
        yaml_node_t *val = yaml_document_get_node(doc, *item);
        if (val && val->type == YAML_SCALAR_NODE && !is_yaml_null(val)) {
            cmds[idx++] = arena_strdup(a, (const char *)val->data.scalar.value);
        }
    }
    cmds[idx] = NULL;
    *count = idx;
    return *count > 0 ? cmds : NULL;
}

static int parse_pane(Arena *a, yaml_document_t *doc, yaml_node_t *node, Pane *pane) {
    memset(pane, 0, sizeof(Pane));

    if (node->type == YAML_SCALAR_NODE) {
        /* Simple pane: just a command string (or null for empty pane) */
        if (!is_yaml_null(node)) {
            const char *val = (const char *)node->data.scalar.value;
            pane->commands = arena_alloc(a, sizeof(char *) * 2);
            pane->commands[0] = arena_strdup(a, val);
            pane->commands[1] = NULL;
            pane->command_count = 1;
        }
        return 0;
    }

    if (node->type == YAML_SEQUENCE_NODE) {
        /* Array of commands for this pane */
        pane->commands = collect_commands(a, doc, node, &pane->command_count);
        return 0;
    }

    if (node->type == YAML_MAPPING_NODE) {
        /* Named pane: { title: [commands] } */
        for (yaml_node_pair_t *pair = node->data.mapping.pairs.start;
             pair < node->data.mapping.pairs.top; pair++) {
            yaml_node_t *key = yaml_document_get_node(doc, pair->key);
            yaml_node_t *val = yaml_document_get_node(doc, pair->value);
            if (!key || !val) continue;
            if (key->type != YAML_SCALAR_NODE) continue;

            if (!is_yaml_null(key)) {
                pane->title = arena_strdup(a, (const char *)key->data.scalar.value);
            }
            if (val->type == YAML_SCALAR_NODE) {
                if (!is_yaml_null(val)) {
                    const char *sv = (const char *)val->data.scalar.value;
                    pane->commands = arena_alloc(a, sizeof(char *) * 2);
                    pane->commands[0] = arena_strdup(a, sv);
                    pane->commands[1] = NULL;
                    pane->command_count = 1;
                }
            } else if (val->type == YAML_SEQUENCE_NODE) {
                pane->commands = collect_commands(a, doc, val, &pane->command_count);
            }
            break; /* only first key */
        }
        return 0;
    }

    return 0;
}

static int parse_window(Arena *a, yaml_document_t *doc, yaml_node_t *node, Window *win) {
    memset(win, 0, sizeof(Window));

    if (node->type != YAML_MAPPING_NODE) return -1;

    /* Each window is a mapping with a single key (window name) -> mapping or scalar */
    for (yaml_node_pair_t *pair = node->data.mapping.pairs.start;
         pair < node->data.mapping.pairs.top; pair++) {
        yaml_node_t *key = yaml_document_get_node(doc, pair->key);
        yaml_node_t *val = yaml_document_get_node(doc, pair->value);
        if (!key || !val) continue;
        if (key->type != YAML_SCALAR_NODE) continue;

        if (!is_yaml_null(key)) {
            win->name = arena_strdup(a, (const char *)key->data.scalar.value);
        }

        if (val->type == YAML_SCALAR_NODE) {
            /* Single command window: "- server: bundle exec rails s" */
            win->panes = arena_alloc(a, sizeof(Pane));
            memset(win->panes, 0, sizeof(Pane));
            win->pane_count = 1;
            if (!is_yaml_null(val)) {
                const char *sv = (const char *)val->data.scalar.value;
                win->panes[0].commands = arena_alloc(a, sizeof(char *) * 2);
                win->panes[0].commands[0] = arena_strdup(a, sv);
                win->panes[0].commands[1] = NULL;
                win->panes[0].command_count = 1;
            }
        } else if (val->type == YAML_SEQUENCE_NODE) {
            /* Array of commands treated as a single pane with multiple commands */
            win->panes = arena_alloc(a, sizeof(Pane));
            memset(win->panes, 0, sizeof(Pane));
            win->pane_count = 1;
            win->panes[0].commands = collect_commands(a, doc, val, &win->panes[0].command_count);
        } else if (val->type == YAML_MAPPING_NODE) {
            /* Full window definition with panes, layout, etc. */
            for (yaml_node_pair_t *wp = val->data.mapping.pairs.start;
                 wp < val->data.mapping.pairs.top; wp++) {
                yaml_node_t *wk = yaml_document_get_node(doc, wp->key);
                yaml_node_t *wv = yaml_document_get_node(doc, wp->value);
                if (!wk || !wv || wk->type != YAML_SCALAR_NODE) continue;

                const char *wkey = (const char *)wk->data.scalar.value;

                if (strcmp(wkey, "layout") == 0 && wv->type == YAML_SCALAR_NODE &&
                    !is_yaml_null(wv)) {
                    win->layout = arena_strdup(a, (const char *)wv->data.scalar.value);
                } else if (strcmp(wkey, "root") == 0 && wv->type == YAML_SCALAR_NODE &&
                           !is_yaml_null(wv)) {
                    win->root = arena_strdup(a, (const char *)wv->data.scalar.value);
                } else if (strcmp(wkey, "focused_pane") == 0 && wv->type == YAML_SCALAR_NODE &&
                           !is_yaml_null(wv)) {
                    win->focused_pane = arena_strdup(a, (const char *)wv->data.scalar.value);
                } else if (strcmp(wkey, "pre") == 0) {
                    win->pre = join_yaml_sequence(a, doc, wv, " && ");
                } else if (strcmp(wkey, "synchronize") == 0 && wv->type == YAML_SCALAR_NODE &&
                           !is_yaml_null(wv)) {
                    bool sync_bool = false;
                    if (try_parse_yaml_bool(wv, &sync_bool)) {
                        if (sync_bool) {
                            win->synchronize = arena_strdup(a, "before");
                        }
                    } else {
                        win->synchronize = arena_strdup(a, (const char *)wv->data.scalar.value);
                    }
                } else if (strcmp(wkey, "panes") == 0 && wv->type == YAML_SEQUENCE_NODE) {
                    int n = (int)(wv->data.sequence.items.top - wv->data.sequence.items.start);
                    if (n > 0) {
                        win->panes = arena_alloc(a, sizeof(Pane) * (size_t)n);
                        win->pane_count = 0;
                        for (yaml_node_item_t *item = wv->data.sequence.items.start;
                             item < wv->data.sequence.items.top; item++) {
                            yaml_node_t *pnode = yaml_document_get_node(doc, *item);
                            if (!pnode) continue;
                            parse_pane(a, doc, pnode, &win->panes[win->pane_count]);
                            win->pane_count++;
                        }
                    }
                }
            }
            /* If no panes were defined, create one empty pane */
            if (win->pane_count == 0) {
                win->panes = arena_alloc(a, sizeof(Pane));
                memset(win->panes, 0, sizeof(Pane));
                win->pane_count = 1;
            }
        }

        break; /* only first key in window mapping */
    }
    return 0;
}

static int parse_document(Arena *a, yaml_document_t *doc, Project *p, const char *override_name) {
    yaml_node_t *root = yaml_document_get_root_node(doc);
    if (!root || root->type != YAML_MAPPING_NODE) {
        fprintf(stderr, "mux: config root is not a mapping\n");
        return -1;
    }

    for (yaml_node_pair_t *pair = root->data.mapping.pairs.start;
         pair < root->data.mapping.pairs.top; pair++) {
        yaml_node_t *key = yaml_document_get_node(doc, pair->key);
        yaml_node_t *val = yaml_document_get_node(doc, pair->value);
        if (!key || !val || key->type != YAML_SCALAR_NODE) continue;

        const char *raw_key = (const char *)key->data.scalar.value;
        const char *k = config_canonical_key(raw_key);
        if (!k) continue; /* ignored/deprecated */

        if (strcmp(k, "name") == 0 && val->type == YAML_SCALAR_NODE && !is_yaml_null(val)) {
            p->name = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "root") == 0 && val->type == YAML_SCALAR_NODE && !is_yaml_null(val)) {
            p->root = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "pre_window") == 0) {
            p->pre_window = join_yaml_sequence(a, doc, val, " && ");
        } else if (strcmp(k, "tmux_command") == 0 && val->type == YAML_SCALAR_NODE &&
                   !is_yaml_null(val)) {
            p->tmux_command = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "tmux_options") == 0 && val->type == YAML_SCALAR_NODE &&
                   !is_yaml_null(val)) {
            p->tmux_options = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "socket_name") == 0 && val->type == YAML_SCALAR_NODE &&
                   !is_yaml_null(val)) {
            p->socket_name = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "socket_path") == 0 && val->type == YAML_SCALAR_NODE &&
                   !is_yaml_null(val)) {
            p->socket_path = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "startup_window") == 0 && val->type == YAML_SCALAR_NODE &&
                   !is_yaml_null(val)) {
            p->startup_window = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "startup_pane") == 0 && val->type == YAML_SCALAR_NODE &&
                   !is_yaml_null(val)) {
            int sp = -1;
            if (parse_nonnegative_int((const char *)val->data.scalar.value, &sp)) {
                p->startup_pane = sp;
            } else if (((const char *)val->data.scalar.value)[0] != '\0') {
                /* Present but not a valid index (e.g. -1). Keep it distinct from
                 * the "unspecified" default (-1) so focus falls to pane 0,
                 * not to focused_pane. Mirrors MUX_STARTUP_PANE_INVALID in
                 * src/script.c. */
                p->startup_pane = -2;
            }
        } else if (strcmp(k, "attach") == 0 && val->type == YAML_SCALAR_NODE) {
            bool attach_val = true;
            if (try_parse_yaml_bool(val, &attach_val)) {
                p->attach = attach_val;
            }
        } else if (strcmp(k, "enable_pane_titles") == 0 && val->type == YAML_SCALAR_NODE) {
            bool titles_val = false;
            if (try_parse_yaml_bool(val, &titles_val)) {
                p->enable_pane_titles = titles_val;
            }
        } else if (strcmp(k, "pane_title_format") == 0 && val->type == YAML_SCALAR_NODE &&
                   !is_yaml_null(val)) {
            p->pane_title_format = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "pane_title_position") == 0 && val->type == YAML_SCALAR_NODE &&
                   !is_yaml_null(val)) {
            p->pane_title_position = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "on_project_start") == 0) {
            p->on_project_start = join_yaml_sequence(a, doc, val, "\n");
        } else if (strcmp(k, "on_project_first_start") == 0) {
            p->on_project_first_start = join_yaml_sequence(a, doc, val, "\n");
        } else if (strcmp(k, "on_project_restart") == 0) {
            p->on_project_restart = join_yaml_sequence(a, doc, val, "\n");
        } else if (strcmp(k, "on_project_exit") == 0) {
            p->on_project_exit = join_yaml_sequence(a, doc, val, "\n");
        } else if (strcmp(k, "on_project_stop") == 0) {
            p->on_project_stop = join_yaml_sequence(a, doc, val, "\n");
        } else if (strcmp(k, "windows") == 0 && val->type == YAML_SEQUENCE_NODE) {
            int n = (int)(val->data.sequence.items.top - val->data.sequence.items.start);
            if (n > 0) {
                p->windows = arena_alloc(a, sizeof(Window) * (size_t)n);
                p->window_count = 0;
                for (yaml_node_item_t *item = val->data.sequence.items.start;
                     item < val->data.sequence.items.top; item++) {
                    yaml_node_t *wnode = yaml_document_get_node(doc, *item);
                    if (!wnode) continue;
                    if (parse_window(a, doc, wnode, &p->windows[p->window_count]) == 0) {
                        p->window_count++;
                    }
                }
            }
        }
    }

    if (override_name && override_name[0]) {
        p->name = arena_strdup(a, override_name);
    }

    if (!p->name || !p->name[0]) {
        fprintf(stderr, "mux: config is missing required 'name' field\n");
        return -1;
    }

    if (p->window_count <= 0) {
        fprintf(stderr, "mux: config must define at least one window\n");
        return -1;
    }

    return 0;
}

int config_parse_string_ex(Arena *a, const char *yaml, size_t yaml_len, Project *p,
                           const char **settings, int setting_count, const char **args,
                           int arg_count, const char *override_name) {
    project_init(p);
    if (!yaml) {
        fprintf(stderr, "mux: empty config\n");
        return -1;
    }

    /* Always run template pass so unset @settings, ENV, and @args are resolved */
    char *processed =
        template_substitute_ex(a, yaml, yaml_len, settings, setting_count, args, arg_count);
    size_t processed_len = strlen(processed);

    yaml_parser_t parser;
    yaml_document_t doc;

    if (!yaml_parser_initialize(&parser)) {
        fprintf(stderr, "mux: failed to initialise YAML parser\n");
        return -1;
    }

    yaml_parser_set_input_string(&parser, (const unsigned char *)processed, processed_len);

    if (!yaml_parser_load(&parser, &doc)) {
        fprintf(stderr, "mux: YAML parse error at line %lu: %s\n",
                (unsigned long)parser.problem_mark.line + 1, parser.problem);
        yaml_parser_delete(&parser);
        return -1;
    }

    int result = parse_document(a, &doc, p, override_name);

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);

    return result;
}

int config_parse_string(Arena *a, const char *yaml, size_t yaml_len, Project *p,
                        const char **settings, int setting_count) {
    return config_parse_string_ex(a, yaml, yaml_len, p, settings, setting_count, NULL, 0, NULL);
}

int config_parse_ex(Arena *a, const char *filepath, Project *p, const char **settings,
                    int setting_count, const char **args, int arg_count,
                    const char *override_name) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "mux: cannot open %s: ", filepath);
        perror(NULL);
        return -1;
    }

    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fprintf(stderr, "mux: cannot stat %s: ", filepath);
        perror(NULL);
        fclose(f);
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        fprintf(stderr, "mux: cannot read %s: is a directory\n", filepath);
        fclose(f);
        return -1;
    }

    char *content = NULL;
    size_t nread = 0;

    if (S_ISREG(st.st_mode)) {
        size_t fsize = (size_t)st.st_size;
        content = arena_alloc(a, fsize + 1);
        nread = fread(content, 1, fsize, f);
        if (ferror(f)) {
            fprintf(stderr, "mux: error reading %s\n", filepath);
            fclose(f);
            return -1;
        }
        content[nread] = '\0';
    } else {
        Str buf = str_new();
        char chunk[4096];
        size_t r;
        while ((r = fread(chunk, 1, sizeof(chunk), f)) > 0) {
            str_appendn(&buf, chunk, r);
        }
        if (ferror(f)) {
            fprintf(stderr, "mux: error reading %s\n", filepath);
            str_free(&buf);
            fclose(f);
            return -1;
        }
        nread = buf.len;
        content = arena_strndup(a, str_cstr(&buf), nread);
        str_free(&buf);
    }

    fclose(f);

    return config_parse_string_ex(a, content, nread, p, settings, setting_count, args, arg_count,
                                  override_name);
}

int config_parse(Arena *a, const char *filepath, Project *p, const char **settings,
                 int setting_count) {
    return config_parse_ex(a, filepath, p, settings, setting_count, NULL, 0, NULL);
}
