#include "cli.h"

#include <stdio.h>
#include <string.h>

static Command parse_command(const char *cmd) {
    if (strcmp(cmd, "start") == 0 || strcmp(cmd, "s") == 0) return CMD_START;
    if (strcmp(cmd, "stop") == 0) return CMD_STOP;
    if (strcmp(cmd, "new") == 0 || strcmp(cmd, "n") == 0) return CMD_NEW;
    if (strcmp(cmd, "edit") == 0 || strcmp(cmd, "e") == 0 || strcmp(cmd, "open") == 0 ||
        strcmp(cmd, "o") == 0)
        return CMD_EDIT;
    if (strcmp(cmd, "copy") == 0 || strcmp(cmd, "c") == 0 || strcmp(cmd, "cp") == 0)
        return CMD_COPY;
    if (strcmp(cmd, "delete") == 0 || strcmp(cmd, "d") == 0 || strcmp(cmd, "rm") == 0)
        return CMD_DELETE;
    if (strcmp(cmd, "list") == 0 || strcmp(cmd, "l") == 0 || strcmp(cmd, "ls") == 0)
        return CMD_LIST;
    if (strcmp(cmd, "debug") == 0) return CMD_DEBUG;
    if (strcmp(cmd, "local") == 0) return CMD_LOCAL;
    if (strcmp(cmd, "doctor") == 0) return CMD_DOCTOR;
    if (strcmp(cmd, "implode") == 0 || strcmp(cmd, "i") == 0) return CMD_IMPLODE;
    if (strcmp(cmd, "stop-all") == 0) return CMD_STOP_ALL;
    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "v") == 0) return CMD_VERSION;
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) return CMD_HELP;
    if (strcmp(cmd, "completions") == 0) return CMD_COMPLETIONS;
    return CMD_NONE;
}

static int parse_bool_str(const char *val, int *out) {
    if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 || strcmp(val, "yes") == 0) {
        *out = 1;
        return 1;
    }
    if (strcmp(val, "false") == 0 || strcmp(val, "0") == 0 || strcmp(val, "no") == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

static int parse_flag_value(int argc, char **argv, int *i, const char *opt_name,
                            const char *inline_val, const char **out_val) {
    if (inline_val) {
        *out_val = inline_val;
        return 0;
    }
    if (*i + 1 >= argc) {
        fprintf(stderr, "mux: option '%s' requires an argument\n", opt_name);
        return -1;
    }
    *out_val = argv[++(*i)];
    return 0;
}

int cli_parse(int argc, char **argv, CliArgs *args) {
    memset(args, 0, sizeof(CliArgs));
    args->attach_override = -1;

    if (argc < 2) {
        args->command = CMD_HELP;
        return 0;
    }

    /* Check for --version and --help before command parsing */
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        args->command = CMD_VERSION;
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        args->command = CMD_HELP;
        return 0;
    }

    int start_idx = 2;
    args->command = parse_command(argv[1]);
    if (args->command == CMD_NONE) {
        /* Implicit start: either `mux <project> ...` or `mux --flag ... <project>` */
        args->command = CMD_START;
        start_idx = 1;
    }

    const char *positionals[CLI_MAX_EXTRA_ARGS];
    int pos_count = 0;
    int end_of_opts = 0;

    for (int i = start_idx; i < argc; i++) {
        const char *arg = argv[i];

        if (!end_of_opts && strcmp(arg, "--") == 0) {
            end_of_opts = 1;
            continue;
        }

        if (!end_of_opts && arg[0] == '-' && arg[1] != '\0') {
            if (strcmp(arg, "-a") == 0 || strcmp(arg, "--append") == 0) {
                args->append = true;
            } else if (strcmp(arg, "-A") == 0 || strcmp(arg, "--active") == 0) {
                args->active_only = true;
            } else if (strcmp(arg, "--attach") == 0) {
                args->attach_override = 1;
            } else if (strncmp(arg, "--attach=", 9) == 0) {
                int bv = 1;
                if (!parse_bool_str(arg + 9, &bv)) {
                    fprintf(stderr, "mux: invalid value for --attach: '%s'\n", arg + 9);
                    return -1;
                }
                args->attach_override = bv;
            } else if (strcmp(arg, "--no-attach") == 0) {
                args->attach_override = 0;
            } else if (strcmp(arg, "--no-pre-window") == 0) {
                args->no_pre_window = true;
            } else if (strcmp(arg, "--local") == 0) {
                args->local_flag = true;
            } else if (strcmp(arg, "-b") == 0 || strcmp(arg, "--backend") == 0 ||
                       strncmp(arg, "--backend=", 10) == 0) {
                const char *val = (strncmp(arg, "--backend=", 10) == 0) ? (arg + 10) : NULL;
                if (parse_flag_value(argc, argv, &i, "--backend", val, &args->backend) != 0) {
                    return -1;
                }
            } else if (strcmp(arg, "-n") == 0 || strcmp(arg, "--name") == 0 ||
                       strncmp(arg, "--name=", 7) == 0) {
                const char *val = (strncmp(arg, "--name=", 7) == 0) ? (arg + 7) : NULL;
                if (parse_flag_value(argc, argv, &i, "--name", val, &args->override_name) != 0) {
                    return -1;
                }
            } else if (strcmp(arg, "-p") == 0 || strcmp(arg, "--project-config") == 0 ||
                       strncmp(arg, "--project-config=", 17) == 0) {
                const char *val = (strncmp(arg, "--project-config=", 17) == 0) ? (arg + 17) : NULL;
                if (parse_flag_value(argc, argv, &i, "--project-config", val,
                                     &args->project_config) != 0) {
                    return -1;
                }
            } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
                args->command = CMD_HELP;
                return 0;
            } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
                args->command = CMD_VERSION;
                return 0;
            } else {
                fprintf(stderr, "mux: unknown option '%s'\n", arg);
                return -1;
            }
            continue;
        }

        if (pos_count < CLI_MAX_EXTRA_ARGS) {
            positionals[pos_count++] = arg;
        }
    }

    int p_idx = 0;

    if (args->command == CMD_LOCAL) {
        /* All positional args for `mux local` are template settings / @args */
    } else if (args->command == CMD_COMPLETIONS) {
        if (p_idx < pos_count) {
            args->completion_shell = positionals[p_idx++];
        }
    } else if (args->command == CMD_DELETE) {
        while (p_idx < pos_count && args->delete_count < CLI_MAX_EXTRA_ARGS) {
            args->delete_targets[args->delete_count++] = positionals[p_idx++];
        }
        if (args->delete_count > 0) {
            args->project_name = args->delete_targets[0];
        }
    } else if (args->command == CMD_COPY) {
        if (p_idx < pos_count) {
            args->project_name = positionals[p_idx++];
        }
        if (p_idx < pos_count) {
            args->copy_target = positionals[p_idx++];
        }
    } else if ((args->command == CMD_START || args->command == CMD_STOP ||
                args->command == CMD_DEBUG) &&
               args->project_config != NULL) {
        /* When --project-config is given, all positional args are template settings / @args */
    } else {
        if (p_idx < pos_count) {
            args->project_name = positionals[p_idx++];
        }
    }

    /* Remaining positional args are template settings (key=value) or @args */
    while (p_idx < pos_count && args->setting_count < CLI_MAX_EXTRA_ARGS) {
        args->settings_storage[args->setting_count++] = positionals[p_idx++];
    }
    if (args->setting_count > 0) {
        args->settings = args->settings_storage;
    }

    return 0;
}

void cli_usage(void) {
    printf("mux %s - a fast tmuxinator replacement\n\n", MUX_VERSION);
    printf("Usage:\n");
    printf("  mux <command> [options] [project] [key=value ...]\n\n");
    printf("Commands:\n");
    printf("  start, s [project]         Start a session or Herdr space (uses local config if "
           "omitted)\n");
    printf("  stop [project]             Stop a session or Herdr space (uses local config if "
           "omitted)\n");
    printf("  new, n [project]           Create a new project config (use --local for "
           "./.tmuxinator.yml)\n");
    printf("  edit, e, open, o <proj>    Edit a project config\n");
    printf("  copy, cp, c <src> <dst>    Copy a project config\n");
    printf("  delete, rm, d <proj ...>   Delete one or more project configs\n");
    printf("  list, ls, l                List available projects\n");
    printf("  debug [project]            Print generated backend script\n");
    printf("  local                      Start from ./.tmuxinator.yml or ./.tmuxinator.yaml\n");
    printf("  doctor                     Check dependencies\n");
    printf("  implode, i                 Delete all configs\n");
    printf("  stop-all                   Stop all active mux project sessions\n");
    printf("  completions <shell>        Print shell completion script\n");
    printf("  version, v                 Print version\n");
    printf("  help, h                    Show this help\n");
    printf("\nOptions:\n");
    printf("  -a, --append               Add windows to existing tmux session\n");
    printf("  -b, --backend NAME         Backend: tmux, herdr, or herdr-app\n");
    printf("  -n, --name NAME            Override session name\n");
    printf("  -p, --project-config PATH  Specify config file path\n");
    printf("  -A, --active               Only list active sessions (for list)\n");
    printf("      --attach, --no-attach  Force attach or detach on start\n");
    printf("      --no-pre-window        Skip project pre_window commands\n");
    printf("      --local                Create ./.tmuxinator.yml (for new)\n");
    printf(
        "  Options may appear before or after the project name. MUX_BACKEND sets the default.\n");
    printf("\nShortcut:\n");
    printf("  mux <project>              Same as mux start <project>\n");
}
