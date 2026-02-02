#include "greatest.h"
#include "arena.h"
#include "config.h"
#include "project.h"
#include "script.h"

#include <stdlib.h>
#include <string.h>

static const char *SIMPLE_CONFIG =
    "name: test\n"
    "root: ~/projects/test\n"
    "windows:\n"
    "  - editor: vim\n"
    "  - server: bundle exec rails s\n";

static const char *HOOKS_CONFIG =
    "name: hooked\n"
    "root: ~/\n"
    "on_project_start: echo starting\n"
    "on_project_stop: echo stopping\n"
    "on_project_exit: echo exiting\n"
    "windows:\n"
    "  - main: echo hi\n";

static const char *MULTI_PANE_CONFIG =
    "name: multi\n"
    "root: ~/\n"
    "windows:\n"
    "  - work:\n"
    "      layout: main-vertical\n"
    "      panes:\n"
    "        - vim\n"
    "        - guard\n"
    "        - tail -f log/dev.log\n";

TEST test_script_start_contains_session(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, SIMPLE_CONFIG, strlen(SIMPLE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "new-session") != NULL);
    ASSERT(strstr(script, "-s test") != NULL);
    ASSERT(strstr(script, "has-session -t test") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_start_contains_windows(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, SIMPLE_CONFIG, strlen(SIMPLE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    ASSERT(strstr(script, "new-window") != NULL);
    ASSERT(strstr(script, "-n server") != NULL);
    ASSERT(strstr(script, "send-keys") != NULL);
    ASSERT(strstr(script, "vim") != NULL);
    ASSERT(strstr(script, "bundle exec rails s") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_start_contains_attach(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, SIMPLE_CONFIG, strlen(SIMPLE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    ASSERT(strstr(script, "attach-session") != NULL);
    ASSERT(strstr(script, "switch-client") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_start_hooks(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, HOOKS_CONFIG, strlen(HOOKS_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    ASSERT(strstr(script, "echo starting") != NULL);
    ASSERT(strstr(script, "echo exiting") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_start_multi_pane(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, MULTI_PANE_CONFIG, strlen(MULTI_PANE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    /* Should have splitw for second and third panes */
    ASSERT(strstr(script, "splitw") != NULL);
    ASSERT(strstr(script, "select-layout") != NULL);
    ASSERT(strstr(script, "main-vertical") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_start_is_valid_bash(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, SIMPLE_CONFIG, strlen(SIMPLE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    /* Should start with shebang */
    ASSERT(strncmp(script, "#!/usr/bin/env bash", 19) == 0);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_stop(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, HOOKS_CONFIG, strlen(HOOKS_CONFIG), &p, NULL, 0);

    char *script = script_generate_stop(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "echo stopping") != NULL);
    ASSERT(strstr(script, "kill-session -t hooked") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_stop_simple(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, SIMPLE_CONFIG, strlen(SIMPLE_CONFIG), &p, NULL, 0);

    char *script = script_generate_stop(&p);
    ASSERT(strstr(script, "kill-session -t test") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

SUITE(script_suite) {
    RUN_TEST(test_script_start_contains_session);
    RUN_TEST(test_script_start_contains_windows);
    RUN_TEST(test_script_start_contains_attach);
    RUN_TEST(test_script_start_hooks);
    RUN_TEST(test_script_start_multi_pane);
    RUN_TEST(test_script_start_is_valid_bash);
    RUN_TEST(test_script_stop);
    RUN_TEST(test_script_stop_simple);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(script_suite);
    GREATEST_MAIN_END();
}
