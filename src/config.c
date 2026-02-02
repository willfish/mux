#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include "str.h"
#include "template.h"

/* Map deprecated field names to canonical names */
static const char *config_canonical_key(const char *key) {
    if (strcmp(key, "project_name") == 0) return "name";
    if (strcmp(key, "project_root") == 0) return "root";
    if (strcmp(key, "cli_args") == 0) return "tmux_options";
    if (strcmp(key, "tabs") == 0) return "windows";
    if (strcmp(key, "rbenv") == 0) return NULL;  /* ignored */
    if (strcmp(key, "rvm") == 0) return NULL;    /* ignored */
    if (strcmp(key, "pre") == 0) return NULL;    /* deprecated, ignored */
    if (strcmp(key, "post") == 0) return NULL;   /* deprecated, ignored */
    return key;
}

/* Join a YAML sequence of strings with "; " */
static char *join_yaml_sequence(Arena *a, yaml_document_t *doc, yaml_node_t *node) {
    if (node->type == YAML_SCALAR_NODE) {
        return arena_strdup(a, (const char *)node->data.scalar.value);
    }
    if (node->type != YAML_SEQUENCE_NODE) return NULL;

    Str buf = str_new();
    int first = 1;
    for (yaml_node_item_t *item = node->data.sequence.items.start;
         item < node->data.sequence.items.top; item++) {
        yaml_node_t *val = yaml_document_get_node(doc, *item);
        if (val && val->type == YAML_SCALAR_NODE) {
            if (!first) str_append(&buf, "; ");
            str_append(&buf, (const char *)val->data.scalar.value);
            first = 0;
        }
    }
    char *result = arena_strdup(a, str_cstr(&buf));
    str_free(&buf);
    return result;
}

/* Collect a YAML sequence (or scalar) as an array of strings */
static char **collect_commands(Arena *a, yaml_document_t *doc, yaml_node_t *node, int *count) {
    *count = 0;

    if (node->type == YAML_SCALAR_NODE) {
        const char *val = (const char *)node->data.scalar.value;
        if (val[0] == '\0') return NULL;
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
        if (val && val->type == YAML_SCALAR_NODE) {
            const char *sv = (const char *)val->data.scalar.value;
            if (sv[0] != '\0') {
                cmds[idx++] = arena_strdup(a, sv);
            }
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
        const char *val = (const char *)node->data.scalar.value;
        if (val[0] != '\0' && strcmp(val, "~") != 0) {
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

            pane->title = arena_strdup(a, (const char *)key->data.scalar.value);
            if (val->type == YAML_SCALAR_NODE) {
                const char *sv = (const char *)val->data.scalar.value;
                if (sv[0] != '\0') {
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

    /* Each window is a mapping with a single key (window name) → mapping or scalar */
    for (yaml_node_pair_t *pair = node->data.mapping.pairs.start;
         pair < node->data.mapping.pairs.top; pair++) {
        yaml_node_t *key = yaml_document_get_node(doc, pair->key);
        yaml_node_t *val = yaml_document_get_node(doc, pair->value);
        if (!key || !val) continue;
        if (key->type != YAML_SCALAR_NODE) continue;

        win->name = arena_strdup(a, (const char *)key->data.scalar.value);

        if (val->type == YAML_SCALAR_NODE) {
            /* Single command window: "- server: bundle exec rails s" */
            const char *sv = (const char *)val->data.scalar.value;
            win->panes = arena_alloc(a, sizeof(Pane));
            memset(win->panes, 0, sizeof(Pane));
            win->pane_count = 1;
            if (sv[0] != '\0') {
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

                if (strcmp(wkey, "layout") == 0 && wv->type == YAML_SCALAR_NODE) {
                    win->layout = arena_strdup(a, (const char *)wv->data.scalar.value);
                } else if (strcmp(wkey, "root") == 0 && wv->type == YAML_SCALAR_NODE) {
                    win->root = arena_strdup(a, (const char *)wv->data.scalar.value);
                } else if (strcmp(wkey, "pre") == 0) {
                    win->pre = join_yaml_sequence(a, doc, wv);
                } else if (strcmp(wkey, "synchronize") == 0 && wv->type == YAML_SCALAR_NODE) {
                    const char *sv = (const char *)wv->data.scalar.value;
                    /* Handle boolean true/false as "before" for compat */
                    if (strcmp(sv, "true") == 0) {
                        win->synchronize = arena_strdup(a, "before");
                    } else if (strcmp(sv, "false") == 0) {
                        /* no sync */
                    } else {
                        win->synchronize = arena_strdup(a, sv);
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

static int parse_document(Arena *a, yaml_document_t *doc, Project *p) {
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

        if (strcmp(k, "name") == 0 && val->type == YAML_SCALAR_NODE) {
            p->name = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "root") == 0 && val->type == YAML_SCALAR_NODE) {
            p->root = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "pre_window") == 0) {
            p->pre_window = join_yaml_sequence(a, doc, val);
        } else if (strcmp(k, "tmux_command") == 0 && val->type == YAML_SCALAR_NODE) {
            p->tmux_command = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "tmux_options") == 0 && val->type == YAML_SCALAR_NODE) {
            p->tmux_options = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "socket_name") == 0 && val->type == YAML_SCALAR_NODE) {
            p->socket_name = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "socket_path") == 0 && val->type == YAML_SCALAR_NODE) {
            p->socket_path = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "startup_window") == 0 && val->type == YAML_SCALAR_NODE) {
            p->startup_window = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "startup_pane") == 0 && val->type == YAML_SCALAR_NODE) {
            p->startup_pane = atoi((const char *)val->data.scalar.value);
        } else if (strcmp(k, "attach") == 0 && val->type == YAML_SCALAR_NODE) {
            const char *sv = (const char *)val->data.scalar.value;
            p->attach = !(strcmp(sv, "false") == 0 || strcmp(sv, "0") == 0);
        } else if (strcmp(k, "enable_pane_titles") == 0 && val->type == YAML_SCALAR_NODE) {
            const char *sv = (const char *)val->data.scalar.value;
            p->enable_pane_titles = (strcmp(sv, "true") == 0 || strcmp(sv, "1") == 0);
        } else if (strcmp(k, "pane_title_format") == 0 && val->type == YAML_SCALAR_NODE) {
            p->pane_title_format = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "pane_title_position") == 0 && val->type == YAML_SCALAR_NODE) {
            p->pane_title_position = arena_strdup(a, (const char *)val->data.scalar.value);
        } else if (strcmp(k, "on_project_start") == 0) {
            p->on_project_start = join_yaml_sequence(a, doc, val);
        } else if (strcmp(k, "on_project_first_start") == 0) {
            p->on_project_first_start = join_yaml_sequence(a, doc, val);
        } else if (strcmp(k, "on_project_restart") == 0) {
            p->on_project_restart = join_yaml_sequence(a, doc, val);
        } else if (strcmp(k, "on_project_exit") == 0) {
            p->on_project_exit = join_yaml_sequence(a, doc, val);
        } else if (strcmp(k, "on_project_stop") == 0) {
            p->on_project_stop = join_yaml_sequence(a, doc, val);
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

    return 0;
}

int config_parse_string(Arena *a, const char *yaml, size_t yaml_len, Project *p,
                        const char **settings, int setting_count) {
    project_init(p);

    /* Template substitution pass */
    char *processed = NULL;
    if (settings && setting_count > 0) {
        processed = template_substitute(a, yaml, settings, setting_count);
    } else {
        processed = arena_strndup(a, yaml, yaml_len);
    }
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

    int result = parse_document(a, &doc, p);

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);

    return result;
}

int config_parse(Arena *a, const char *filepath, Project *p, const char **settings,
                 int setting_count) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "mux: cannot open %s: ", filepath);
        perror(NULL);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0) {
        fprintf(stderr, "mux: cannot read %s\n", filepath);
        fclose(f);
        return -1;
    }

    char *content = arena_alloc(a, (size_t)fsize + 1);
    size_t nread = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[nread] = '\0';

    return config_parse_string(a, content, nread, p, settings, setting_count);
}
