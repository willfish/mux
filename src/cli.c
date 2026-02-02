#include "cli.h"

#include <getopt.h>
#include <stdio.h>
#include <string.h>

static Command parse_command(const char *cmd) {
    if (strcmp(cmd, "start") == 0 || strcmp(cmd, "s") == 0) return CMD_START;
    if (strcmp(cmd, "stop") == 0) return CMD_STOP;
    if (strcmp(cmd, "new") == 0 || strcmp(cmd, "n") == 0) return CMD_NEW;
    if (strcmp(cmd, "copy") == 0 || strcmp(cmd, "c") == 0) return CMD_COPY;
    if (strcmp(cmd, "delete") == 0 || strcmp(cmd, "d") == 0) return CMD_DELETE;
    if (strcmp(cmd, "list") == 0 || strcmp(cmd, "l") == 0 || strcmp(cmd, "ls") == 0)
        return CMD_LIST;
    if (strcmp(cmd, "debug") == 0) return CMD_DEBUG;
    if (strcmp(cmd, "local") == 0) return CMD_LOCAL;
    if (strcmp(cmd, "doctor") == 0) return CMD_DOCTOR;
    if (strcmp(cmd, "implode") == 0) return CMD_IMPLODE;
    if (strcmp(cmd, "stop-all") == 0) return CMD_STOP_ALL;
    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "v") == 0) return CMD_VERSION;
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) return CMD_HELP;
    if (strcmp(cmd, "completions") == 0) return CMD_COMPLETIONS;
    return CMD_NONE;
}

int cli_parse(int argc, char **argv, CliArgs *args) {
    memset(args, 0, sizeof(CliArgs));

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

    args->command = parse_command(argv[1]);
    if (args->command == CMD_NONE) {
        /* Treat unknown command as a project name for start */
        args->command = CMD_START;
        args->project_name = argv[1];

        /* Collect remaining args as settings */
        if (argc > 2) {
            args->settings = (const char **)&argv[2];
            args->setting_count = argc - 2;
        }
        return 0;
    }

    /* Parse remaining arguments based on command */
    static struct option long_opts[] = {
        {"append", no_argument, 0, 'a'},
        {"name", required_argument, 0, 'n'},
        {"project-config", required_argument, 0, 'p'},
        {"active", no_argument, 0, 'A'},
        {0, 0, 0, 0},
    };

    /* Reset getopt */
    optind = 2;

    int opt;
    while ((opt = getopt_long(argc, argv, "+an:p:A", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'a':
            args->append = true;
            break;
        case 'n':
            args->override_name = optarg;
            break;
        case 'p':
            args->project_config = optarg;
            break;
        case 'A':
            args->active_only = true;
            break;
        default:
            break;
        }
    }

    /* Collect positional args after options */
    if (optind < argc) {
        args->project_name = argv[optind];
        optind++;
    }

    /* For copy command, second positional arg is the target */
    if (args->command == CMD_COPY && optind < argc) {
        args->copy_target = argv[optind];
        optind++;
    }

    /* For completions command, the arg is the shell name */
    if (args->command == CMD_COMPLETIONS && args->project_name) {
        args->completion_shell = args->project_name;
        args->project_name = NULL;
    }

    /* Remaining args are settings (key=value pairs) */
    if (optind < argc) {
        args->settings = (const char **)&argv[optind];
        args->setting_count = argc - optind;
    }

    return 0;
}

void cli_usage(void) {
    printf("mux %s — a fast tmuxinator replacement\n\n", MUX_VERSION);
    printf("Usage:\n");
    printf("  mux <command> [options] [project] [key=value ...]\n\n");
    printf("Commands:\n");
    printf("  start, s <project>       Start a tmux session\n");
    printf("  stop <project>           Stop a tmux session\n");
    printf("  new, n [project]         Create a new project config\n");
    printf("  copy, c <src> <dst>      Copy a project config\n");
    printf("  delete, d <project...>   Delete project configs\n");
    printf("  list, ls, l              List available projects\n");
    printf("  debug <project>          Print generated tmux script\n");
    printf("  local                    Start from ./.tmuxinator.yml\n");
    printf("  doctor                   Check dependencies\n");
    printf("  implode                  Delete all configs\n");
    printf("  stop-all                 Stop all tmux sessions\n");
    printf("  completions <shell>      Print shell completion script\n");
    printf("  version, v               Print version\n");
    printf("  help, h                  Show this help\n");
    printf("\nOptions:\n");
    printf("  -a, --append             Add windows to existing session\n");
    printf("  -n, --name NAME          Override session name\n");
    printf("  -p, --project-config P   Specify config file path\n");
    printf("  -A, --active             Only list active sessions (for list)\n");
    printf("\nShortcut:\n");
    printf("  mux <project>            Same as mux start <project>\n");
}
