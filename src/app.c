#include "app.h"

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "arena.h"
#include "cli.h"
#include "completion.h"
#include "config.h"
#include "doctor.h"
#include "path.h"
#include "project.h"
#include "script.h"
#include "shell.h"
#include "str.h"
#include "template.h"
#include "tmux.h"

static const char *selected_backend(const CliArgs *args) {
    if (args->backend && args->backend[0]) return args->backend;
    const char *env_backend = getenv("MUX_BACKEND");
    if (env_backend && env_backend[0]) return env_backend;
    return "tmux";
}

static char *generate_project_script(const Project *p, const CliArgs *args, int stop) {
    const char *backend = selected_backend(args);
    ScriptOptions opts = {
        .append = args->append,
        .attach_override = args->attach_override,
        .no_pre_window = args->no_pre_window,
    };

    if (strcmp(backend, "tmux") == 0) {
        return stop ? script_generate_stop(p) : script_generate_start_ex(p, &opts);
    }
    if (strcmp(backend, "herdr") == 0) {
        return stop ? script_generate_stop_herdr(p) : script_generate_start_herdr_ex(p, &opts);
    }
    if (strcmp(backend, "herdr-app") == 0) {
        return stop ? script_generate_stop_herdr_app(p)
                    : script_generate_start_herdr_app_ex(p, &opts);
    }
    fprintf(stderr, "mux: unknown backend '%s' (use tmux, herdr, or herdr-app)\n", backend);
    return NULL;
}

static const char *DEFAULT_CONFIG_TEMPLATE = "# %s\n"
                                             "\n"
                                             "name: %s\n"
                                             "root: ~/\n"
                                             "\n"
                                             "windows:\n"
                                             "  - editor:\n"
                                             "      layout: main-vertical\n"
                                             "      panes:\n"
                                             "        - vim\n"
                                             "        -\n"
                                             "  - server: echo 'start server'\n";

static int read_confirmation(void) {
    char line[64];
    if (!fgets(line, sizeof(line), stdin)) {
        return 0;
    }
    return line[0] == 'y' || line[0] == 'Y';
}

/* Split extra CLI args into key=value settings pairs and positional @args. */
static void parse_template_inputs(Arena *a, const CliArgs *args, const char ***settings,
                                  int *setting_count, const char ***pos_args, int *pos_arg_count) {
    *settings = NULL;
    *setting_count = 0;
    *pos_args = NULL;
    *pos_arg_count = 0;
    if (args->setting_count == 0) return;

    const char **pairs = arena_alloc(a, sizeof(char *) * (size_t)(args->setting_count * 2));
    const char **pargs = arena_alloc(a, sizeof(char *) * (size_t)args->setting_count);
    int n_pairs = 0;
    int n_args = 0;

    for (int i = 0; i < args->setting_count; i++) {
        char *key = NULL, *value = NULL;
        if (template_parse_setting(a, args->settings[i], &key, &value) == 0) {
            pairs[n_pairs++] = key;
            pairs[n_pairs++] = value;
        } else {
            pargs[n_args++] = args->settings[i];
        }
    }

    if (n_pairs > 0) {
        *settings = pairs;
        *setting_count = n_pairs;
    }
    if (n_args > 0) {
        *pos_args = pargs;
        *pos_arg_count = n_args;
    }
}

static int ensure_parent_dir(const char *filepath) {
    char *copy = strdup(filepath);
    if (!copy) return -1;
    char *parent = dirname(copy);
    int ret = path_mkdir_p(parent);
    free(copy);
    return ret;
}

static int load_project(Arena *a, const CliArgs *args, Project *p) {
    const char *filepath = NULL;

    if (args->project_config) {
        filepath = args->project_config;
    } else if (args->project_name) {
        filepath = path_find_project(a, args->project_name);
    } else {
        filepath = path_find_local(a);
    }

    if (!filepath) {
        if (args->project_name) {
            fprintf(stderr, "mux: project '%s' not found\n", args->project_name);
        } else {
            fprintf(stderr, "mux: no project specified and no local .tmuxinator.yml found\n");
        }
        return -1;
    }

    const char **settings = NULL;
    int setting_count = 0;
    const char **pos_args = NULL;
    int pos_arg_count = 0;
    parse_template_inputs(a, args, &settings, &setting_count, &pos_args, &pos_arg_count);

    if (config_parse_ex(a, filepath, p, settings, setting_count, pos_args, pos_arg_count,
                        args->override_name) != 0) {
        return -1;
    }

    return 0;
}

static int cmd_start(Arena *a, const CliArgs *args) {
    Project p;
    if (load_project(a, args, &p) != 0) return 1;

    char *script = generate_project_script(&p, args, 0);
    if (!script) {
        fprintf(stderr, "mux: failed to generate start script\n");
        return 1;
    }

    int ret = shell_exec_bash(script);
    free(script);
    return ret;
}

static int cmd_stop(Arena *a, const CliArgs *args) {
    Project p;
    if (load_project(a, args, &p) != 0) return 1;

    char *script = generate_project_script(&p, args, 1);
    if (!script) {
        fprintf(stderr, "mux: failed to generate stop script\n");
        return 1;
    }

    int ret = shell_exec_bash(script);
    free(script);
    return ret;
}

static int cmd_debug(Arena *a, const CliArgs *args) {
    Project p;
    if (load_project(a, args, &p) != 0) return 1;

    char *script = generate_project_script(&p, args, 0);
    if (!script) {
        fprintf(stderr, "mux: failed to generate script\n");
        return 1;
    }

    printf("%s", script);
    free(script);
    return 0;
}

static int cmd_new(Arena *a, const CliArgs *args) {
    const char *name = args->project_name;
    char *filepath = NULL;

    if (args->local_flag) {
        filepath = arena_strdup(a, ".tmuxinator.yml");
        if (!name || !name[0]) {
            char cwd[4096];
            if (getcwd(cwd, sizeof(cwd))) {
                char *cwd_copy = arena_strdup(a, cwd);
                name = arena_strdup(a, basename(cwd_copy));
            } else {
                name = "project";
            }
        }
    } else {
        if (!name || !name[0]) {
            fprintf(stderr, "mux: please specify a project name\n");
            return 1;
        }
        filepath = path_project_file(a, name);
        if (!filepath) return 1;

        if (ensure_parent_dir(filepath) != 0) {
            fprintf(stderr, "mux: cannot create parent directory for %s: ", filepath);
            perror(NULL);
            return 1;
        }
    }

    /* Check if file already exists */
    struct stat st;
    if (stat(filepath, &st) == 0) {
        fprintf(stderr, "mux: project '%s' already exists at %s\n", name, filepath);
        return 1;
    }

    FILE *f = fopen(filepath, "w");
    if (!f) {
        fprintf(stderr, "mux: cannot create %s: ", filepath);
        perror(NULL);
        return 1;
    }
    fprintf(f, DEFAULT_CONFIG_TEMPLATE, filepath, name);
    if (fclose(f) != 0) {
        fprintf(stderr, "mux: error writing %s: ", filepath);
        perror(NULL);
        return 1;
    }

    printf("Created %s\n", filepath);

    /* Open in editor if $EDITOR is set */
    const char *editor = getenv("EDITOR");
    if (editor && editor[0]) {
        return shell_exec(shell_editor_command(a, editor, filepath));
    }

    return 0;
}

static int cmd_edit(Arena *a, const CliArgs *args) {
    const char *name = args->project_name;
    if (!name) {
        fprintf(stderr, "mux: please specify a project name\n");
        return 1;
    }

    char *filepath = path_find_project(a, name);
    if (!filepath) {
        fprintf(stderr, "mux: project '%s' not found\n", name);
        return 1;
    }

    const char *editor = getenv("EDITOR");
    if (!editor || !editor[0]) {
        editor = "vi";
    }

    return shell_exec(shell_editor_command(a, editor, filepath));
}

static int cmd_copy(Arena *a, const CliArgs *args) {
    if (!args->project_name || !args->copy_target) {
        fprintf(stderr, "mux: usage: mux copy <existing> <new>\n");
        return 1;
    }

    char *src = path_find_project(a, args->project_name);
    if (!src) {
        fprintf(stderr, "mux: project '%s' not found\n", args->project_name);
        return 1;
    }

    char *dst = path_project_file(a, args->copy_target);
    if (!dst) return 1;

    if (ensure_parent_dir(dst) != 0) {
        fprintf(stderr, "mux: cannot create parent directory for %s: ", dst);
        perror(NULL);
        return 1;
    }

    /* Check if target already exists */
    struct stat st;
    if (stat(dst, &st) == 0) {
        fprintf(stderr, "mux: project '%s' already exists\n", args->copy_target);
        return 1;
    }

    FILE *in = fopen(src, "r");
    if (!in) {
        fprintf(stderr, "mux: cannot open %s: ", src);
        perror(NULL);
        return 1;
    }
    FILE *out = fopen(dst, "w");
    if (!out) {
        fclose(in);
        fprintf(stderr, "mux: cannot create %s: ", dst);
        perror(NULL);
        return 1;
    }

    char buf[4096];
    size_t n;
    int io_err = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            io_err = 1;
            break;
        }
    }
    if (ferror(in) || ferror(out)) {
        io_err = 1;
    }
    fclose(in);
    if (fclose(out) != 0) {
        io_err = 1;
    }
    if (io_err) {
        unlink(dst);
        fprintf(stderr, "mux: error copying %s to %s\n", src, dst);
        return 1;
    }

    printf("Copied %s to %s\n", args->project_name, args->copy_target);
    return 0;
}

static int cmd_delete(Arena *a, const CliArgs *args) {
    if (args->delete_count == 0) {
        fprintf(stderr, "mux: please specify project(s) to delete\n");
        return 1;
    }

    int status = 0;
    for (int i = 0; i < args->delete_count; i++) {
        const char *name = args->delete_targets[i];
        char *filepath = path_find_project(a, name);
        if (!filepath) {
            fprintf(stderr, "mux: project '%s' not found\n", name);
            status = 1;
            continue;
        }

        printf("Delete %s? (y/n) ", filepath);
        fflush(stdout);
        if (!read_confirmation()) {
            printf("Aborted.\n");
            continue;
        }

        if (unlink(filepath) != 0) {
            fprintf(stderr, "mux: cannot delete %s: ", filepath);
            perror(NULL);
            status = 1;
            continue;
        }

        printf("Deleted %s\n", filepath);
    }

    return status;
}

static int project_uses_custom_tmux(const Project *p) {
    /* Non-empty tmux_options (e.g. "-L isolated") can select a different
     * server, so list --active / stop-all must probe it directly instead of
     * matching against the default server's session list. */
    return (p->socket_name && p->socket_name[0]) || (p->socket_path && p->socket_path[0]) ||
           (p->tmux_options && p->tmux_options[0]) ||
           (p->tmux_command && p->tmux_command[0] && strcmp(p->tmux_command, "tmux") != 0);
}

static int project_is_active_tmux(Arena *a, const char *project_name, char **default_sessions,
                                  int default_session_count, Project *out_project) {
    char *filepath = path_find_project(a, project_name);
    Project parsed;
    int has_parsed = 0;

    if (filepath && config_parse(a, filepath, &parsed, NULL, 0) == 0) {
        has_parsed = 1;
        if (out_project) {
            *out_project = parsed;
        }
    }

    if (has_parsed && project_uses_custom_tmux(&parsed)) {
        return tmux_project_has_session(a, &parsed);
    }

    const char *raw_name = has_parsed ? parsed.name : project_name;
    char *session_name = tmux_sanitize_session_name(a, raw_name);
    return tmux_session_names_contain(default_sessions, default_session_count, session_name);
}

static int cmd_list(Arena *a, const CliArgs *args) {
    int count = 0;
    char **projects = path_list_projects(a, &count);
    if (!projects) return 0;

    if (args->active_only) {
        int session_count = 0;
        char **sessions = tmux_list_sessions(a, &session_count);
        for (int i = 0; i < count; i++) {
            if (project_is_active_tmux(a, projects[i], sessions, session_count, NULL)) {
                printf("%s\n", projects[i]);
            }
        }
    } else {
        for (int i = 0; i < count; i++) {
            printf("%s\n", projects[i]);
        }
    }
    return 0;
}

static int cmd_local(Arena *a, const CliArgs *args) {
    char *filepath = path_find_local(a);
    if (!filepath) {
        fprintf(stderr, "mux: no .tmuxinator.yml found in current directory\n");
        return 1;
    }

    const char **settings = NULL;
    int setting_count = 0;
    const char **pos_args = NULL;
    int pos_arg_count = 0;
    parse_template_inputs(a, args, &settings, &setting_count, &pos_args, &pos_arg_count);

    Project p;
    if (config_parse_ex(a, filepath, &p, settings, setting_count, pos_args, pos_arg_count,
                        args->override_name) != 0) {
        return 1;
    }

    char *script = generate_project_script(&p, args, 0);
    if (!script) return 1;

    int ret = shell_exec_bash(script);
    free(script);
    return ret;
}

static int cmd_implode(Arena *a) {
    char *config_dir = path_config_dir(a);
    if (!config_dir) return 1;

    printf("This will delete all configs in %s. Continue? (y/n) ", config_dir);
    fflush(stdout);
    if (!read_confirmation()) {
        printf("Aborted.\n");
        return 0;
    }

    /* Only delete configs in the confirmed directory, matching upstream
     * (Tmuxinator::Config.directory). Projects found in other config
     * directories (e.g. ~/.tmuxinator) must survive. */
    int count = 0;
    char **projects = path_list_projects_in(a, config_dir, &count);
    if (!projects) return 0;

    static const char *exts[] = {".yml", ".yaml"};
    for (int i = 0; i < count; i++) {
        for (size_t e = 0; e < sizeof(exts) / sizeof(exts[0]); e++) {
            Str filepath = str_new();
            str_appendf(&filepath, "%s/%s%s", config_dir, projects[i], exts[e]);
            if (unlink(str_cstr(&filepath)) == 0) {
                printf("Deleted %s\n", str_cstr(&filepath));
            } else if (errno != ENOENT) {
                fprintf(stderr, "mux: cannot delete %s: ", str_cstr(&filepath));
                perror(NULL);
            }
            str_free(&filepath);
        }
    }

    return 0;
}

static int cmd_stop_all(Arena *a, const CliArgs *args) {
    int count = 0;
    char **projects = path_list_projects(a, &count);
    if (!projects || count == 0) {
        printf("No active mux sessions.\n");
        return 0;
    }

    int session_count = 0;
    char **sessions = tmux_list_sessions(a, &session_count);

    Project *active = arena_alloc(a, sizeof(Project) * (size_t)count);
    int active_count = 0;

    for (int i = 0; i < count; i++) {
        Project p;
        memset(&p, 0, sizeof(p));
        if (project_is_active_tmux(a, projects[i], sessions, session_count, &p) && p.name) {
            active[active_count++] = p;
        }
    }

    if (active_count == 0) {
        printf("No active mux sessions.\n");
        return 0;
    }

    printf("Stop %d active mux session(s)? (y/n) ", active_count);
    fflush(stdout);
    if (!read_confirmation()) {
        printf("Aborted.\n");
        return 0;
    }

    int status = 0;
    for (int i = 0; i < active_count; i++) {
        char *script = generate_project_script(&active[i], args, 1);
        if (!script) {
            status = 1;
            continue;
        }
        if (shell_exec_bash(script) != 0) {
            status = 1;
        }
        free(script);
    }

    return status;
}

static int cmd_completions(const CliArgs *args) {
    if (!args->completion_shell) {
        fprintf(stderr, "mux: please specify a shell: bash, zsh, or fish\n");
        return 1;
    }
    if (strcmp(args->completion_shell, "bash") == 0) {
        completion_bash();
    } else if (strcmp(args->completion_shell, "zsh") == 0) {
        completion_zsh();
    } else if (strcmp(args->completion_shell, "fish") == 0) {
        completion_fish();
    } else {
        fprintf(stderr, "mux: unknown shell '%s' (use bash, zsh, or fish)\n",
                args->completion_shell);
        return 1;
    }
    return 0;
}

int mux_run(int argc, char **argv) {
    CliArgs args;
    if (cli_parse(argc, argv, &args) != 0) {
        return 1;
    }

    Arena a = arena_new();
    int ret = 0;

    switch (args.command) {
    case CMD_VERSION:
        printf("mux %s\n", MUX_VERSION);
        break;
    case CMD_HELP:
        cli_usage();
        break;
    case CMD_START:
        ret = cmd_start(&a, &args);
        break;
    case CMD_STOP:
        ret = cmd_stop(&a, &args);
        break;
    case CMD_DEBUG:
        ret = cmd_debug(&a, &args);
        break;
    case CMD_NEW:
        ret = cmd_new(&a, &args);
        break;
    case CMD_EDIT:
        ret = cmd_edit(&a, &args);
        break;
    case CMD_COPY:
        ret = cmd_copy(&a, &args);
        break;
    case CMD_DELETE:
        ret = cmd_delete(&a, &args);
        break;
    case CMD_LIST:
        ret = cmd_list(&a, &args);
        break;
    case CMD_LOCAL:
        ret = cmd_local(&a, &args);
        break;
    case CMD_DOCTOR:
        ret = doctor_check();
        break;
    case CMD_IMPLODE:
        ret = cmd_implode(&a);
        break;
    case CMD_STOP_ALL:
        ret = cmd_stop_all(&a, &args);
        break;
    case CMD_COMPLETIONS:
        ret = cmd_completions(&args);
        break;
    case CMD_NONE:
        cli_usage();
        ret = 1;
        break;
    }

    arena_free(&a);
    return ret;
}
