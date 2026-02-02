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

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(config_suite);
    GREATEST_MAIN_END();
}
