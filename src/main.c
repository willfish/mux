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
#include "template.h"

static const char *DEFAULT_CONFIG_TEMPLATE =
    "# ~/.config/tmuxinator/%s.yml\n"
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

/* Parse settings from CLI args into key/value pairs for template substitution */
static void parse_settings(Arena *a, const CliArgs *args, const char ***settings, int *count) {
    *settings = NULL;
    *count = 0;
    if (args->setting_count == 0) return;

    /* Each setting is key=value, we store as [key, value, key, value, ...] */
    const char **pairs = arena_alloc(a, sizeof(char *) * (size_t)(args->setting_count * 2));
    int n = 0;
    for (int i = 0; i < args->setting_count; i++) {
        char *key = NULL, *value = NULL;
        if (template_parse_setting(a, args->settings[i], &key, &value) == 0) {
            pairs[n++] = key;
            pairs[n++] = value;
        }
    }
    *settings = pairs;
    *count = n;
}

static int load_project(Arena *a, const CliArgs *args, Project *p) {
    const char *filepath = NULL;

    if (args->project_config) {
        filepath = args->project_config;
    } else if (args->project_name) {
        filepath = path_find_project(a, args->project_name);
    }

    if (!filepath) {
        if (args->project_name) {
            fprintf(stderr, "mux: project '%s' not found\n", args->project_name);
        } else {
            fprintf(stderr, "mux: no project specified\n");
        }
        return -1;
    }

    const char **settings = NULL;
    int setting_count = 0;
    parse_settings(a, args, &settings, &setting_count);

    if (config_parse(a, filepath, p, settings, setting_count) != 0) {
        return -1;
    }

    /* Override session name if --name was given */
    if (args->override_name) {
        p->name = arena_strdup(a, args->override_name);
    }

    return 0;
}

static int cmd_start(Arena *a, const CliArgs *args) {
    Project p;
    if (load_project(a, args, &p) != 0) return 1;

    char *script = script_generate_start(&p);
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

    char *script = script_generate_stop(&p);
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

    char *script = script_generate_start(&p);
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
    if (!name) {
        fprintf(stderr, "mux: please specify a project name\n");
        return 1;
    }

    char *filepath = path_project_file(a, name);
    if (!filepath) return 1;

    /* Check config dir exists, create if needed */
    char *config_dir = path_config_dir(a);
    if (config_dir) {
        struct stat st;
        if (stat(config_dir, &st) != 0) {
            mkdir(config_dir, 0755);
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
    fprintf(f, DEFAULT_CONFIG_TEMPLATE, name, name);
    fclose(f);

    printf("Created %s\n", filepath);

    /* Open in editor */
    const char *editor = getenv("EDITOR");
    if (editor && editor[0]) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "%s '%s'", editor, filepath);
        return shell_exec(cmd);
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

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "%s '%s'", editor, filepath);
    return shell_exec(cmd);
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

    /* Check if target already exists */
    struct stat st;
    if (stat(dst, &st) == 0) {
        fprintf(stderr, "mux: project '%s' already exists\n", args->copy_target);
        return 1;
    }

    /* Copy file */
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
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);

    printf("Copied %s to %s\n", args->project_name, args->copy_target);
    return 0;
}

static int cmd_delete(Arena *a, const CliArgs *args) {
    if (!args->project_name) {
        fprintf(stderr, "mux: please specify project(s) to delete\n");
        return 1;
    }

    char *filepath = path_find_project(a, args->project_name);
    if (!filepath) {
        fprintf(stderr, "mux: project '%s' not found\n", args->project_name);
        return 1;
    }

    printf("Delete %s? (y/n) ", filepath);
    fflush(stdout);
    int c = getchar();
    if (c != 'y' && c != 'Y') {
        printf("Aborted.\n");
        return 0;
    }

    if (unlink(filepath) != 0) {
        fprintf(stderr, "mux: cannot delete %s: ", filepath);
        perror(NULL);
        return 1;
    }

    printf("Deleted %s\n", filepath);
    return 0;
}

static int cmd_list(Arena *a, const CliArgs *args) {
    if (args->active_only) {
        /* List only sessions that are currently running */
        int count = 0;
        char **projects = path_list_projects(a, &count);
        if (!projects) return 0;

        for (int i = 0; i < count; i++) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "tmux has-session -t %s 2>/dev/null", projects[i]);
            if (system(cmd) == 0) {
                printf("%s\n", projects[i]);
            }
        }
    } else {
        int count = 0;
        char **projects = path_list_projects(a, &count);
        if (!projects) return 0;

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
    parse_settings(a, args, &settings, &setting_count);

    Project p;
    if (config_parse(a, filepath, &p, settings, setting_count) != 0) {
        return 1;
    }

    if (args->override_name) {
        p.name = arena_strdup(a, args->override_name);
    }

    char *script = script_generate_start(&p);
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
    int c = getchar();
    if (c != 'y' && c != 'Y') {
        printf("Aborted.\n");
        return 0;
    }

    int count = 0;
    char **projects = path_list_projects(a, &count);
    if (!projects) return 0;

    for (int i = 0; i < count; i++) {
        char *filepath = path_project_file(a, projects[i]);
        if (filepath) {
            if (unlink(filepath) == 0) {
                printf("Deleted %s\n", filepath);
            } else {
                fprintf(stderr, "mux: cannot delete %s: ", filepath);
                perror(NULL);
            }
        }
    }

    return 0;
}

static int cmd_stop_all(void) {
    printf("Kill all tmux sessions? (y/n) ");
    fflush(stdout);
    int c = getchar();
    if (c != 'y' && c != 'Y') {
        printf("Aborted.\n");
        return 0;
    }
    return shell_exec("tmux kill-server");
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

int main(int argc, char **argv) {
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
        ret = cmd_stop_all();
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
