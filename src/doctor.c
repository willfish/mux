#include "doctor.h"

#include <stdio.h>
#include <stdlib.h>

static int check_command(const char *name, const char *test_cmd) {
    int ret = system(test_cmd);
    if (ret == 0) {
        printf("  [ok] %s\n", name);
        return 0;
    } else {
        printf("  [MISSING] %s\n", name);
        return -1;
    }
}

int doctor_check(void) {
    int errors = 0;

    printf("Checking dependencies...\n\n");

    if (check_command("tmux", "which tmux >/dev/null 2>&1") != 0)
        errors++;

    if (check_command("bash", "which bash >/dev/null 2>&1") != 0)
        errors++;

    const char *editor = getenv("EDITOR");
    if (editor && editor[0]) {
        printf("  [ok] $EDITOR = %s\n", editor);
    } else {
        printf("  [warn] $EDITOR not set\n");
    }

    const char *shell = getenv("SHELL");
    if (shell && shell[0]) {
        printf("  [ok] $SHELL = %s\n", shell);
    } else {
        printf("  [warn] $SHELL not set\n");
    }

    printf("\n");

    if (errors > 0) {
        printf("Some dependencies are missing.\n");
        return -1;
    }
    printf("All dependencies satisfied.\n");
    return 0;
}
