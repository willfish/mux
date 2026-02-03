#include "greatest.h"
#include "arena.h"
#include "config.h"
#include "project.h"

#include <string.h>

static const char *SIMPLE_CONFIG =
    "name: test\n"
    "root: ~/projects/test\n"
    "windows:\n"
    "  - editor: vim\n"
    "  - server: bundle exec rails s\n";

static const char *FULL_CONFIG =
    "name: myproject\n"
    "root: ~/code\n"
    "tmux_command: tmux\n"
    "tmux_options: -f ~/.tmux.conf\n"
    "socket_name: myproject\n"
    "pre_window: nvm use 18\n"
    "startup_window: editor\n"
    "startup_pane: 1\n"
    "attach: true\n"
    "on_project_start: echo starting\n"
    "on_project_stop: echo stopping\n"
    "windows:\n"
    "  - editor:\n"
    "      layout: main-vertical\n"
    "      panes:\n"
    "        - vim\n"
    "        - guard\n"
    "  - server: bundle exec rails s\n"
    "  - logs: tail -f log/development.log\n";

static const char *PANE_CONFIG =
    "name: panes\n"
    "root: ~/\n"
    "windows:\n"
    "  - work:\n"
    "      panes:\n"
    "        - cd /tmp && ls\n"
    "        -\n"
    "        - top\n";

static const char *DEPRECATED_CONFIG =
    "project_name: oldstyle\n"
    "project_root: ~/old\n"
    "tabs:\n"
    "  - main: echo hello\n";

static const char *HOOKS_ARRAY_CONFIG =
    "name: hooks\n"
    "root: ~/\n"
    "on_project_start:\n"
    "  - echo first\n"
    "  - echo second\n"
    "windows:\n"
    "  - main: echo hi\n";

static const char *WINDOW_ROOT_CONFIG =
    "name: roots\n"
    "root: ~/default\n"
    "windows:\n"
    "  - frontend:\n"
    "      root: ~/frontend\n"
    "      panes:\n"
    "        - npm start\n"
    "  - backend:\n"
    "      panes:\n"
    "        - rails s\n";

static const char *EMPTY_PANE_CONFIG =
    "name: empty\n"
    "windows:\n"
    "  - shell:\n"
    "      panes:\n"
    "        -\n"
    "        - ~\n";

static const char *SYNC_CONFIG =
    "name: sync\n"
    "windows:\n"
    "  - synced:\n"
    "      synchronize: after\n"
    "      panes:\n"
    "        - echo 1\n"
    "        - echo 2\n";

TEST test_config_simple(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse_string(&a, SIMPLE_CONFIG, strlen(SIMPLE_CONFIG), &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("test", p.name);
    ASSERT_STR_EQ("~/projects/test", p.root);
    ASSERT_EQ(2, p.window_count);
    ASSERT_STR_EQ("editor", p.windows[0].name);
    ASSERT_STR_EQ("server", p.windows[1].name);
    ASSERT_EQ(1, p.windows[0].pane_count);
    ASSERT_EQ(1, p.windows[0].panes[0].command_count);
    ASSERT_STR_EQ("vim", p.windows[0].panes[0].commands[0]);
    ASSERT_STR_EQ("bundle exec rails s", p.windows[1].panes[0].commands[0]);
    arena_free(&a);
    PASS();
}

TEST test_config_full(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse_string(&a, FULL_CONFIG, strlen(FULL_CONFIG), &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("myproject", p.name);
    ASSERT_STR_EQ("~/code", p.root);
    ASSERT_STR_EQ("tmux", p.tmux_command);
    ASSERT_STR_EQ("-f ~/.tmux.conf", p.tmux_options);
    ASSERT_STR_EQ("myproject", p.socket_name);
    ASSERT_STR_EQ("nvm use 18", p.pre_window);
    ASSERT_STR_EQ("editor", p.startup_window);
    ASSERT_EQ(1, p.startup_pane);
    ASSERT(p.attach);
    ASSERT_STR_EQ("echo starting", p.on_project_start);
    ASSERT_STR_EQ("echo stopping", p.on_project_stop);
    ASSERT_EQ(3, p.window_count);
    ASSERT_STR_EQ("main-vertical", p.windows[0].layout);
    ASSERT_EQ(2, p.windows[0].pane_count);
    arena_free(&a);
    PASS();
}

TEST test_config_panes(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse_string(&a, PANE_CONFIG, strlen(PANE_CONFIG), &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1, p.window_count);
    ASSERT_EQ(3, p.windows[0].pane_count);
    ASSERT_EQ(1, p.windows[0].panes[0].command_count);
    ASSERT_STR_EQ("cd /tmp && ls", p.windows[0].panes[0].commands[0]);
    ASSERT_EQ(0, p.windows[0].panes[1].command_count); /* empty pane */
    ASSERT_EQ(1, p.windows[0].panes[2].command_count);
    ASSERT_STR_EQ("top", p.windows[0].panes[2].commands[0]);
    arena_free(&a);
    PASS();
}

TEST test_config_deprecated(void) {
    Arena a = arena_new();
    Project p;
    int ret =
        config_parse_string(&a, DEPRECATED_CONFIG, strlen(DEPRECATED_CONFIG), &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("oldstyle", p.name);
    ASSERT_STR_EQ("~/old", p.root);
    ASSERT_EQ(1, p.window_count);
    ASSERT_STR_EQ("main", p.windows[0].name);
    arena_free(&a);
    PASS();
}

TEST test_config_hooks_array(void) {
    Arena a = arena_new();
    Project p;
    int ret =
        config_parse_string(&a, HOOKS_ARRAY_CONFIG, strlen(HOOKS_ARRAY_CONFIG), &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("echo first; echo second", p.on_project_start);
    arena_free(&a);
    PASS();
}

TEST test_config_window_root(void) {
    Arena a = arena_new();
    Project p;
    int ret =
        config_parse_string(&a, WINDOW_ROOT_CONFIG, strlen(WINDOW_ROOT_CONFIG), &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, p.window_count);
    ASSERT_STR_EQ("~/frontend", p.windows[0].root);
    ASSERT(p.windows[1].root == NULL);
    arena_free(&a);
    PASS();
}

TEST test_config_empty_panes(void) {
    Arena a = arena_new();
    Project p;
    int ret =
        config_parse_string(&a, EMPTY_PANE_CONFIG, strlen(EMPTY_PANE_CONFIG), &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1, p.window_count);
    ASSERT_EQ(2, p.windows[0].pane_count);
    /* Both panes should have 0 commands */
    ASSERT_EQ(0, p.windows[0].panes[0].command_count);
    ASSERT_EQ(0, p.windows[0].panes[1].command_count);
    arena_free(&a);
    PASS();
}

TEST test_config_synchronize(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse_string(&a, SYNC_CONFIG, strlen(SYNC_CONFIG), &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("after", p.windows[0].synchronize);
    arena_free(&a);
    PASS();
}

TEST test_config_defaults(void) {
    Arena a = arena_new();
    Project p;
    const char *minimal = "name: minimal\nwindows:\n  - main: echo hi\n";
    int ret = config_parse_string(&a, minimal, strlen(minimal), &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT(p.attach == true);
    ASSERT(p.tmux_command == NULL);
    ASSERT(p.root == NULL);
    ASSERT(p.pre_window == NULL);
    ASSERT(p.startup_pane == -1);
    arena_free(&a);
    PASS();
}

TEST test_config_with_template(void) {
    Arena a = arena_new();
    const char *yaml =
        "name: <%= @settings[\"project\"] %>\n"
        "root: ~/\n"
        "windows:\n"
        "  - main: echo <%= @settings[\"greeting\"] %>\n";

    const char *settings[] = {"project", "myproj", "greeting", "hello"};
    Project p;
    int ret = config_parse_string(&a, yaml, strlen(yaml), &p, settings, 4);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("myproj", p.name);
    ASSERT_STR_EQ("echo hello", p.windows[0].panes[0].commands[0]);
    arena_free(&a);
    PASS();
}

/* ========== Fixture file tests ========== */

#define FIXTURE_PATH "tests/fixtures/"

TEST test_fixture_sample(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "sample.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("sample", p.name);
    ASSERT_STR_EQ("~/test", p.root);
    ASSERT_STR_EQ("foo", p.socket_name);
    ASSERT_STR_EQ("rbenv shell 2.0.0-p247", p.pre_window);
    ASSERT_STR_EQ("-f ~/.tmux.mac.conf", p.tmux_options);
    ASSERT_EQ(9, p.window_count);
    /* First window: editor with layout and panes */
    ASSERT_STR_EQ("editor", p.windows[0].name);
    ASSERT_STR_EQ("main-vertical", p.windows[0].layout);
    ASSERT_EQ(4, p.windows[0].pane_count);
    /* Window-level pre command */
    ASSERT(p.windows[0].pre != NULL);
    arena_free(&a);
    PASS();
}

TEST test_fixture_sample_deprecations(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "sample_deprecations.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    /* project_name -> name */
    ASSERT_STR_EQ("sample", p.name);
    /* project_root -> root */
    ASSERT_STR_EQ("~/test", p.root);
    /* cli_args -> tmux_options */
    ASSERT_STR_EQ("-f ~/.tmux.mac.conf", p.tmux_options);
    /* tabs -> windows */
    ASSERT(p.window_count >= 1);
    arena_free(&a);
    PASS();
}

TEST test_fixture_detach(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "detach.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(false, p.attach);
    arena_free(&a);
    PASS();
}

TEST test_fixture_sample_wemux(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "sample_wemux.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("wemux", p.tmux_command);
    arena_free(&a);
    PASS();
}

TEST test_fixture_nameless_window(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "nameless_window.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("nameless_window", p.name);
    ASSERT_EQ(2, p.window_count);
    /* First window has ~ as name (could be treated as empty) */
    ASSERT_STR_EQ("other", p.windows[1].name);
    arena_free(&a);
    PASS();
}

TEST test_fixture_noroot(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "noroot.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("noroot", p.name);
    ASSERT(p.root == NULL);
    ASSERT_EQ(1, p.window_count);
    arena_free(&a);
    PASS();
}

TEST test_fixture_sample_number_as_name(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "sample_number_as_name.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("222", p.name);
    arena_free(&a);
    PASS();
}

TEST test_fixture_sample_literals_as_window_name(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "sample_literals_as_window_name.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(6, p.window_count);
    /* Numeric window names should be converted to strings */
    ASSERT_STR_EQ("222", p.windows[0].name);
    ASSERT_STR_EQ("222_333", p.windows[1].name);
    /* Boolean literals as window names */
    ASSERT_STR_EQ("true", p.windows[4].name);
    ASSERT_STR_EQ("false", p.windows[5].name);
    arena_free(&a);
    PASS();
}

TEST test_fixture_hooks(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "hooks.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("echo \"project start\"", p.on_project_start);
    ASSERT_STR_EQ("echo \"first start\"", p.on_project_first_start);
    ASSERT_STR_EQ("echo \"restart\"", p.on_project_restart);
    ASSERT_STR_EQ("echo \"exit\"", p.on_project_exit);
    ASSERT_STR_EQ("echo \"stop\"", p.on_project_stop);
    arena_free(&a);
    PASS();
}

TEST test_fixture_pane_titles(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "pane_titles.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT(p.enable_pane_titles);
    ASSERT_STR_EQ("bottom", p.pane_title_position);
    ASSERT_STR_EQ(" #T ", p.pane_title_format);
    /* Pane titles and commands */
    ASSERT_EQ(3, p.windows[0].pane_count);
    ASSERT_STR_EQ("Editor", p.windows[0].panes[0].title);
    ASSERT_STR_EQ("vim", p.windows[0].panes[0].commands[0]);
    ASSERT_STR_EQ("Shell", p.windows[0].panes[1].title);
    ASSERT_STR_EQ("bash", p.windows[0].panes[1].commands[0]);
    ASSERT_STR_EQ("Logs", p.windows[0].panes[2].title);
    arena_free(&a);
    PASS();
}

TEST test_fixture_synchronize(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "synchronize.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(3, p.window_count);
    ASSERT_STR_EQ("before", p.windows[0].synchronize);
    ASSERT_STR_EQ("after", p.windows[1].synchronize);
    ASSERT(p.windows[2].synchronize == NULL);
    arena_free(&a);
    PASS();
}

TEST test_fixture_startup(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "startup.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("logs", p.startup_window);
    ASSERT_EQ(1, p.startup_pane);
    arena_free(&a);
    PASS();
}

TEST test_fixture_socket(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "socket.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("mysocket", p.socket_name);
    arena_free(&a);
    PASS();
}

TEST test_fixture_window_root(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "window_root.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("~/default", p.root);
    ASSERT_EQ(3, p.window_count);
    ASSERT_STR_EQ("~/", p.windows[0].root);
    ASSERT_STR_EQ("~/projects", p.windows[1].root);
    ASSERT(p.windows[2].root == NULL); /* Uses project root */
    arena_free(&a);
    PASS();
}

TEST test_fixture_template(void) {
    Arena a = arena_new();
    Project p;
    const char *settings[] = {"root", "/tmp/test", "host", "localhost", "port", "3000"};
    int ret = config_parse(&a, FIXTURE_PATH "template.yml", &p, settings, 6);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("/tmp/test", p.root);
    ASSERT_STR_EQ("echo \"host=localhost port=3000\"", p.windows[0].panes[0].commands[0]);
    arena_free(&a);
    PASS();
}

SUITE(config_suite) {
    RUN_TEST(test_config_simple);
    RUN_TEST(test_config_full);
    RUN_TEST(test_config_panes);
    RUN_TEST(test_config_deprecated);
    RUN_TEST(test_config_hooks_array);
    RUN_TEST(test_config_window_root);
    RUN_TEST(test_config_empty_panes);
    RUN_TEST(test_config_synchronize);
    RUN_TEST(test_config_defaults);
    RUN_TEST(test_config_with_template);
}

SUITE(fixture_suite) {
    RUN_TEST(test_fixture_sample);
    RUN_TEST(test_fixture_sample_deprecations);
    RUN_TEST(test_fixture_detach);
    RUN_TEST(test_fixture_sample_wemux);
    RUN_TEST(test_fixture_nameless_window);
    RUN_TEST(test_fixture_noroot);
    RUN_TEST(test_fixture_sample_number_as_name);
    RUN_TEST(test_fixture_sample_literals_as_window_name);
    RUN_TEST(test_fixture_hooks);
    RUN_TEST(test_fixture_pane_titles);
    RUN_TEST(test_fixture_synchronize);
    RUN_TEST(test_fixture_startup);
    RUN_TEST(test_fixture_socket);
    RUN_TEST(test_fixture_window_root);
    RUN_TEST(test_fixture_template);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(config_suite);
    RUN_SUITE(fixture_suite);
    GREATEST_MAIN_END();
}
