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

/* ========== Fixture-based script tests ========== */

#define FIXTURE_PATH "tests/fixtures/"

TEST test_script_detach_no_attach(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "detach.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(false, p.attach);

    char *script = script_generate_start(&p);
    /* When attach is false, should NOT have attach-session */
    ASSERT(strstr(script, "attach-session") == NULL);
    ASSERT(strstr(script, "switch-client") == NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_wemux_command(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "sample_wemux.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *script = script_generate_start(&p);
    /* Should use wemux instead of tmux */
    ASSERT(strstr(script, "wemux") != NULL);
    ASSERT(strstr(script, "start-server") != NULL);
    ASSERT(strstr(script, "new-session") != NULL);
    /* Verify wemux is the command by checking it appears before start-server */
    ASSERT(strstr(script, "wemux -f") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_socket_name(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "socket.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *script = script_generate_start(&p);
    /* Should use -L for socket name */
    ASSERT(strstr(script, "-L mysocket") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_synchronize(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "synchronize.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *script = script_generate_start(&p);
    /* Should set synchronize-panes */
    ASSERT(strstr(script, "synchronize-panes on") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_startup_window_and_pane(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "startup.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *script = script_generate_start(&p);
    /* Should select the startup window */
    ASSERT(strstr(script, "select-window -t startup:logs") != NULL);
    /* Should select the startup pane */
    ASSERT(strstr(script, "select-pane") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_pane_titles(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "pane_titles.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *script = script_generate_start(&p);
    /* Should set pane border status */
    ASSERT(strstr(script, "pane-border-status") != NULL);
    ASSERT(strstr(script, "bottom") != NULL);
    /* Should set pane titles */
    ASSERT(strstr(script, "-T \"Editor\"") != NULL);
    ASSERT(strstr(script, "-T \"Shell\"") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_window_root(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "window_root.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *script = script_generate_start(&p);
    /* Should use window-specific roots */
    ASSERT(strstr(script, "-c ~/") != NULL);
    ASSERT(strstr(script, "-c ~/projects") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_hooks_all(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "hooks.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *start_script = script_generate_start(&p);
    /* on_project_start runs before session check */
    ASSERT(strstr(start_script, "echo \"project start\"") != NULL);
    /* on_project_first_start runs in new session block */
    ASSERT(strstr(start_script, "echo \"first start\"") != NULL);
    /* on_project_restart runs in else block */
    ASSERT(strstr(start_script, "echo \"restart\"") != NULL);
    /* on_project_exit runs at end */
    ASSERT(strstr(start_script, "echo \"exit\"") != NULL);
    free(start_script);

    char *stop_script = script_generate_stop(&p);
    ASSERT(strstr(stop_script, "echo \"stop\"") != NULL);
    free(stop_script);

    arena_free(&a);
    PASS();
}

TEST test_script_sample_comprehensive(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "sample.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *script = script_generate_start(&p);
    /* Socket name */
    ASSERT(strstr(script, "-L foo") != NULL);
    /* tmux options */
    ASSERT(strstr(script, "-f ~/.tmux.mac.conf") != NULL);
    /* pre_window command in send-keys */
    ASSERT(strstr(script, "rbenv shell 2.0.0-p247") != NULL);
    /* Window layout */
    ASSERT(strstr(script, "main-vertical") != NULL);
    ASSERT(strstr(script, "tiled") != NULL);
    /* Multiple windows created */
    ASSERT(strstr(script, "new-window") != NULL);
    /* Pane splits */
    ASSERT(strstr(script, "splitw") != NULL);
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

SUITE(script_fixture_suite) {
    RUN_TEST(test_script_detach_no_attach);
    RUN_TEST(test_script_wemux_command);
    RUN_TEST(test_script_socket_name);
    RUN_TEST(test_script_synchronize);
    RUN_TEST(test_script_startup_window_and_pane);
    RUN_TEST(test_script_pane_titles);
    RUN_TEST(test_script_window_root);
    RUN_TEST(test_script_hooks_all);
    RUN_TEST(test_script_sample_comprehensive);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(script_suite);
    RUN_SUITE(script_fixture_suite);
    GREATEST_MAIN_END();
}
