#ifndef MUX_CLI_H
#define MUX_CLI_H

#include <stdbool.h>

#define MUX_VERSION "0.1.0"

typedef enum {
    CMD_NONE = 0,
    CMD_START,
    CMD_STOP,
    CMD_NEW,
    CMD_EDIT,
    CMD_COPY,
    CMD_DELETE,
    CMD_LIST,
    CMD_DEBUG,
    CMD_LOCAL,
    CMD_DOCTOR,
    CMD_IMPLODE,
    CMD_STOP_ALL,
    CMD_VERSION,
    CMD_HELP,
    CMD_COMPLETIONS,
} Command;

typedef struct {
    Command command;
    const char *project_name;     /* primary project name argument */
    const char *copy_target;      /* second arg for copy command */
    const char *project_config;   /* --project-config path override */
    const char *override_name;    /* --name override */
    const char *completion_shell; /* bash, zsh, or fish */
    bool append;                  /* --append flag */
    bool active_only;             /* --active flag for list */

    /* Template settings: key=value pairs from extra args */
    const char **settings;
    int setting_count;
} CliArgs;

/* Parse argc/argv into a CliArgs struct. Returns 0 on success, -1 on error. */
int cli_parse(int argc, char **argv, CliArgs *args);

/* Print usage to stdout. */
void cli_usage(void);

#endif
