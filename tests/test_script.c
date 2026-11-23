#include "arena.h"
#include "config.h"
#include "greatest.h"
#include "project.h"
#include "script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void rm_rf_tmpdir(const char *dir) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    int rc = system(cmd);
    (void)rc;
}

static const char *SIMPLE_CONFIG = "name: test\n"
                                   "root: ~/projects/test\n"
                                   "windows:\n"
                                   "  - editor: vim\n"
                                   "  - server: bundle exec rails s\n";

static const char *HOOKS_CONFIG = "name: hooked\n"
                                  "root: ~/\n"
                                  "on_project_start: echo starting\n"
                                  "on_project_stop: echo stopping\n"
                                  "on_project_exit: echo exiting\n"
                                  "windows:\n"
                                  "  - main: echo hi\n";

static const char *MULTI_PANE_CONFIG = "name: multi\n"
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
    ASSERT(strstr(script, "has-session -t =test") != NULL);
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
    ASSERT(strstr(script, "if [ -z \"${TMUX:-}\" ]; then") != NULL);
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

TEST test_script_start_normalizes_empty_tmux_indices(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, SIMPLE_CONFIG, strlen(SIMPLE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    ASSERT(strstr(script, "case \"$pane_base_index\" in ''|*[!0-9]*) pane_base_index=0;; esac") !=
           NULL);
    ASSERT(strstr(script, "case \"$base_index\" in ''|*[!0-9]*) base_index=0;; esac") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_start_tiles_after_splitting_panes(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, MULTI_PANE_CONFIG, strlen(MULTI_PANE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    ASSERT(strstr(script, "splitw -t =multi:$((base_index+0))") != NULL);
    ASSERT(strstr(script, "select-layout -t =multi:$((base_index+0)) tiled") != NULL);
    ASSERT(strstr(script, "select-pane -t =multi:$((base_index+0)).$((pane_base_index+0))") !=
           NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_herdr_maps_windows_to_workspace_tabs_and_panes(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, MULTI_PANE_CONFIG, strlen(MULTI_PANE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start_herdr(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "workspace create --focus --cwd ~/ --label multi") != NULL);
    ASSERT(strstr(script, "tab rename \"$tab_0\" work") != NULL);
    ASSERT(
        strstr(script, "pane split \"$pane_0_0\" --direction right --ratio 0.333333 --no-focus") !=
        NULL);
    ASSERT(
        strstr(script, "pane split \"$pane_0_1\" --direction right --ratio 0.500000 --no-focus") !=
        NULL);
    ASSERT(strstr(script, "pane send-text \"$pane_0_0\" vim") != NULL);
    ASSERT(strstr(script, "pane send-text \"$pane_0_1\" guard") != NULL);
    ASSERT(strstr(script, "pane send-text \"$pane_0_2\" 'tail -f log/dev.log'") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_herdr_reports_bad_json_without_python_traceback(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, MULTI_PANE_CONFIG, strlen(MULTI_PANE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start_herdr(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "json.JSONDecodeError") == NULL);
    ASSERT(strstr(script, "except Exception as exc:") != NULL);
    ASSERT(strstr(script, "mux: herdr returned invalid JSON while reading") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_herdr_starts_server_when_missing(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, MULTI_PANE_CONFIG, strlen(MULTI_PANE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start_herdr(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "mux_herdr_server_running()") != NULL);
    ASSERT(strstr(script, "\"$herdr_cmd\" server >/dev/null 2>&1 &") != NULL);
    ASSERT(strstr(script, "mux_herdr_ensure_server") != NULL);
    ASSERT(strstr(script, "requires a running Herdr session") == NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_herdr_stop_closes_matching_workspace(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, HOOKS_CONFIG, strlen(HOOKS_CONFIG), &p, NULL, 0);

    char *script = script_generate_stop_herdr(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "echo stopping") != NULL);
    ASSERT(strstr(script, "workspace close \"$workspace_id\"") != NULL);
    ASSERT(strstr(script, "label = sys.argv[1]") != NULL);
    ASSERT(strstr(script, " hooked | while") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_herdr_app_maps_windows_and_splits_before_commands(void) {
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0,
              config_parse_string(&a, MULTI_PANE_CONFIG, strlen(MULTI_PANE_CONFIG), &p, NULL, 0));

    char *script = script_generate_start_herdr_app(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "do space.new --machine '' --name multi --tab work --folder ~/") != NULL);
    const char *split = strstr(script, "do pane.split --pane \"$pane_0_0\" --direction right");
    const char *send = strstr(script, "mux_app_send \"$terminal_0_0\" ~/ vim");
    ASSERT(split != NULL);
    ASSERT(send != NULL);
    ASSERT(split < send);
    ASSERT(strstr(script, "do pane.split --pane \"$pane_0_1\" --direction right") != NULL);
    ASSERT(strstr(script, "mux_app_send \"$terminal_0_2\" ~/ 'tail -f log/dev.log'") != NULL);
    ASSERT(strstr(script, "printf -v text 'cd -- %q\\n%s'") != NULL);
    ASSERT(strstr(script, "session attach") == NULL);
    ASSERT(strstr(script, " server ") == NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_herdr_app_reuses_only_local_spaces(void) {
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, HOOKS_CONFIG, strlen(HOOKS_CONFIG), &p, NULL, 0));

    char *script = script_generate_start_herdr_app(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "if not s.get(\"machine\") and s[\"name\"] == key") != NULL);
    ASSERT(strstr(script, "more than one local space has that name") != NULL);
    ASSERT(strstr(script, "do space.select --id \"$space_id\"") != NULL);
    ASSERT(strstr(script, "echo starting") < strstr(script, "space_id=$("));
    ASSERT(strstr(script, "echo exiting") != NULL);
    ASSERT(strstr(script, "  exit 0\nfi") != NULL);
    ASSERT(strstr(script, "requires an open Herdr app window") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_herdr_app_roots_hooks_and_startup_focus(void) {
    const char *config = "name: team's work\n"
                         "root: /tmp/mux root\n"
                         "pre_window: echo ready\n"
                         "on_project_first_start: echo first\n"
                         "startup_window: services\n"
                         "startup_pane: 1\n"
                         "windows:\n"
                         "  - postgres: echo \"$HOME\"\n"
                         "  - services:\n"
                         "      root: /tmp/services\n"
                         "      pre: echo services\n"
                         "      panes:\n"
                         "        - echo one\n"
                         "        - echo two\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, config, strlen(config), &p, NULL, 0));

    char *script = script_generate_start_herdr_app(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "--name 'team'\\''s work' --tab postgres --folder '/tmp/mux root'") !=
           NULL);
    ASSERT(
        strstr(script, "do tab.new --space \"$space_id\" --name services --folder /tmp/services") !=
        NULL);
    ASSERT(strstr(script, "echo first") < strstr(script, "do space.new"));
    ASSERT(
        strstr(
            script,
            "mux_app_send \"$terminal_1_1\" /tmp/services 'echo ready\necho services\necho two'") !=
        NULL);
    ASSERT(strstr(script, "do tab.select --id \"$tab_1\"") != NULL);
    ASSERT(strstr(script, "do pane.focus --pane \"$pane_1_1\"") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_herdr_app_stop_closes_named_space_without_core_restart(void) {
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, HOOKS_CONFIG, strlen(HOOKS_CONFIG), &p, NULL, 0));

    char *script = script_generate_stop_herdr_app(&p);
    ASSERT(script != NULL);
    ASSERT(strstr(script, "mux_app_json space hooked") != NULL);
    ASSERT(strstr(script, "echo stopping") != NULL);
    ASSERT(strstr(script, "do space.close --id \"$space_id\" --confirmed true") != NULL);
    ASSERT(strstr(script, "server stop") == NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_quotes_shell_sensitive_tmux_arguments(void) {
    Arena a = arena_new();
    Project p;
    const char *config = "name: team's work\n"
                         "root: /tmp/mux root\n"
                         "enable_pane_titles: true\n"
                         "windows:\n"
                         "  - main window:\n"
                         "      panes:\n"
                         "        - Editor $HOME: echo \"$HOME\"\n";

    int ret = config_parse_string(&a, config, strlen(config), &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *script = script_generate_start(&p);
    ASSERT(strstr(script, "new-session -d -P -F '#{window_index} #{pane_index}' "
                          "-s 'team'\\''s work' "
                          "-x \"${MUX_TMUX_COLUMNS:-120}\" -y \"${MUX_TMUX_LINES:-40}\" "
                          "-n 'main window'") != NULL);
    ASSERT(strstr(script, "-c '/tmp/mux root'") != NULL);
    ASSERT(strstr(script, "has-session -t '=team'\\''s work'") != NULL);
    ASSERT(strstr(script, "select-pane -t '=team'\\''s work':$((base_index+0))."
                          "$((pane_base_index+0)) -T 'Editor $HOME'") != NULL);
    ASSERT(strstr(script, "send-keys -t '=team'\\''s work':$((base_index+0))."
                          "$((pane_base_index+0)) \"echo \\\"\\$HOME\\\"\" C-m") != NULL);
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
    ASSERT(strstr(script, "cd ~/") != NULL);
    ASSERT(strstr(script, "echo stopping") != NULL);
    ASSERT(strstr(script, "kill-session -t =hooked") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_stop_simple(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, SIMPLE_CONFIG, strlen(SIMPLE_CONFIG), &p, NULL, 0);

    char *script = script_generate_stop(&p);
    ASSERT(strstr(script, "kill-session -t =test") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_stop_guards_kill_session_behind_has_session(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, HOOKS_CONFIG, strlen(HOOKS_CONFIG), &p, NULL, 0);

    char *script = script_generate_stop(&p);
    /* The whole stop body (cd, hook, kill-session) runs only when the exact
     * session exists, so a missing session exits 0 silently. */
    const char *guard = strstr(script, "has-session -t =hooked 2>/dev/null; then");
    const char *cd = strstr(script, "cd ~/");
    const char *hook = strstr(script, "echo stopping");
    const char *kill = strstr(script, "kill-session -t =hooked");
    ASSERT(guard != NULL);
    ASSERT(cd != NULL);
    ASSERT(hook != NULL);
    ASSERT(kill != NULL);
    ASSERT(guard < cd);
    ASSERT(cd < hook);
    ASSERT(hook < kill);
    ASSERT(strstr(script, "\nfi\n") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_stop_guards_custom_tmux_base(void) {
    const char *cfg = "name: sock\n"
                      "socket_name: foo\n"
                      "windows:\n"
                      "  - main: echo hi\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char *script = script_generate_stop(&p);
    ASSERT(strstr(script, "tmux -L foo has-session -t =sock 2>/dev/null; then") != NULL);
    ASSERT(strstr(script, "tmux -L foo kill-session -t =sock") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_send_keys_escapes_trailing_semicolon(void) {
    const char *cfg = "name: semi\n"
                      "windows:\n"
                      "  - one: echo semi-ok;\n"
                      "  - two:\n"
                      "      panes:\n"
                      "        - echo a; echo b\n"
                      "        - echo tail \\; done;\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char *script = script_generate_start(&p);
    /* A trailing ';' must reach tmux as the two characters `\;`. */
    ASSERT(strstr(script, "\"echo semi-ok\\;\" C-m") != NULL);
    /* Interior semicolons are not escaped. */
    ASSERT(strstr(script, "\"echo a; echo b\" C-m") != NULL);
    /* Trailing-semicolon escaping composes with existing backslash escaping. */
    ASSERT(strstr(script, "\"echo tail \\\\; done\\;\" C-m") != NULL);
    /* Only the trailing semicolon is escaped. */
    ASSERT(strstr(script, "echo a\\;") == NULL);
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
    /* Should select the startup window by index (logs is window index 2) */
    ASSERT(strstr(script, "select-window -t =startup:$((base_index+2))") != NULL);
    /* Should select the startup pane */
    ASSERT(strstr(script, "select-pane -t =startup:$((base_index+2)).$((pane_base_index+1))") !=
           NULL);
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
    /* Should set window-scoped pane border status (not global -g) */
    ASSERT(strstr(script, "set-option -g pane-border-status") == NULL);
    ASSERT(strstr(script, "set-window-option -t =pane_titles:$((base_index+0)) "
                          "pane-border-status bottom") != NULL);
    /* Should set pane titles */
    ASSERT(strstr(script, "-T \"Editor\"") != NULL);
    ASSERT(strstr(script, "-T \"Shell\"") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_focused_pane(void) {
    Arena a = arena_new();
    Project p;
    int ret = config_parse(&a, FIXTURE_PATH "focused_pane.yml", &p, NULL, 0);
    ASSERT_EQ(0, ret);

    char *script = script_generate_start(&p);
    ASSERT(strstr(script,
                  "select-pane -t =focused_pane:$((base_index+0)).$((pane_base_index+1))") != NULL);
    ASSERT(strstr(script,
                  "select-pane -t =focused_pane:$((base_index+1)).$((pane_base_index+0))") != NULL);
    /* Default pane-border-format is applied when pane_title_format is omitted */
    ASSERT(strstr(script, "pane-border-format '#{pane_index}: #{pane_title}'") != NULL);
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

TEST test_script_sanitizes_dots_and_colons_in_session_name(void) {
    const char *cfg = "name: my.proj:dev\n"
                      "windows:\n"
                      "  - main: echo hi\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char *start = script_generate_start(&p);
    ASSERT(strstr(start, "has-session -t =my_proj_dev") != NULL);
    ASSERT(strstr(start, "-s my_proj_dev") != NULL);
    ASSERT(strstr(start, "attach-session -t =my_proj_dev") != NULL);
    free(start);

    char *stop = script_generate_stop(&p);
    ASSERT(strstr(stop, "kill-session -t =my_proj_dev") != NULL);
    free(stop);
    arena_free(&a);
    PASS();
}

TEST test_script_nameless_window_omits_name_flag_and_uses_index(void) {
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse(&a, FIXTURE_PATH "nameless_window.yml", &p, NULL, 0));

    char *script = script_generate_start(&p);
    ASSERT(strstr(script, " -n ~") == NULL);
    ASSERT(strstr(script, "new-session -d -P -F '#{window_index} #{pane_index}' "
                          "-s nameless_window -x \"${MUX_TMUX_COLUMNS:-120}\" "
                          "-y \"${MUX_TMUX_LINES:-40}\" -c ~/") != NULL);
    ASSERT(
        strstr(script, "send-keys -t =nameless_window:$((base_index+0)).$((pane_base_index+0))") !=
        NULL);
    ASSERT(
        strstr(script, "send-keys -t =nameless_window:$((base_index+1)).$((pane_base_index+0))") !=
        NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_relative_window_root_and_tilde_space(void) {
    const char *cfg = "name: roots\n"
                      "root: ~/my project\n"
                      "windows:\n"
                      "  - api:\n"
                      "      root: services/api\n"
                      "      panes:\n"
                      "        - pwd\n"
                      "  - abs:\n"
                      "      root: /var/log\n"
                      "      panes:\n"
                      "        - pwd\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char *script = script_generate_start(&p);
    ASSERT(strstr(script, "cd ~/'my project'\n") != NULL);
    ASSERT(strstr(script, "-c ~/'my project/services/api'") != NULL);
    ASSERT(strstr(script, "-c /var/log") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_out_of_range_focus_and_startup_pane_fall_back_to_zero(void) {
    const char *cfg = "name: bounds\n"
                      "startup_window: 99\n"
                      "startup_pane: 99\n"
                      "windows:\n"
                      "  - editor:\n"
                      "      focused_pane: 50\n"
                      "      panes:\n"
                      "        - vim\n"
                      "        - bash\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char *tmux_script = script_generate_start(&p);
    /* Strict mode is enabled globally; hooks toggle it off around user code. */
    ASSERT(strstr(tmux_script, "set -euo pipefail") != NULL);
    ASSERT(strstr(tmux_script, "pane_base_index+50") == NULL);
    ASSERT(strstr(tmux_script, "pane_base_index+99") == NULL);
    ASSERT(strstr(tmux_script, "select-pane -t =bounds:$((base_index+0)).$((pane_base_index+0))") !=
           NULL);
    ASSERT(strstr(tmux_script, "select-window -t =bounds:$((base_index+0))") != NULL);
    free(tmux_script);

    char *herdr_script = script_generate_start_herdr(&p);
    ASSERT(strstr(herdr_script, "pane_0_50") == NULL);
    ASSERT(strstr(herdr_script, "pane_0_99") == NULL);
    ASSERT(strstr(herdr_script, "pane focus") == NULL);
    ASSERT(strstr(herdr_script, "--no-focus") != NULL);
    free(herdr_script);
    arena_free(&a);
    PASS();
}

TEST test_script_options_append_no_attach_and_no_pre_window(void) {
    const char *cfg = "name: opts\n"
                      "pre_window: echo PRE_WIN\n"
                      "windows:\n"
                      "  - main: echo hi\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    ScriptOptions opts = {.append = true, .attach_override = 0, .no_pre_window = true};
    char *script = script_generate_start_ex(&p, &opts);
    /* Upstream appends to the CURRENT tmux session via display-message '#S',
     * never to the config's own session, and never attaches. */
    ASSERT(strstr(script, "new-window -P -F '#{window_id}' -t \"=$target_session\"") != NULL);
    ASSERT(strstr(script, "display-message -p '#S'") != NULL);
    ASSERT(strstr(script, "Cannot append to a session that does not exist") != NULL);
    ASSERT(strstr(script, "has-session -t =opts") == NULL);
    ASSERT(strstr(script, "new-session") == NULL);
    ASSERT(strstr(script, "attach-session") == NULL);
    ASSERT(strstr(script, "switch-client") == NULL);
    ASSERT(strstr(script, "PRE_WIN") == NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_start_strict_mode_with_hook_isolation(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, HOOKS_CONFIG, strlen(HOOKS_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    /* Strict mode is global, but each user hook runs without it so unset
     * variables and failing commands in hook code cannot abort the script. */
    ASSERT(strstr(script, "set -euo pipefail") != NULL);
    const char *start_hook = strstr(script, "echo starting");
    ASSERT(start_hook != NULL);
    const char *off = strstr(script, "set +euo pipefail\n");
    ASSERT(off != NULL);
    ASSERT(off < start_hook);
    /* strict mode is restored after the hook */
    const char *on = strstr(start_hook, "set -euo pipefail\n");
    ASSERT(on != NULL);
    ASSERT(start_hook < on);
    /* Non-fatal failures are recorded and reported at the end. */
    ASSERT(strstr(script, "mux_try() {") != NULL);
    ASSERT(strstr(script, "mux_errors=0") != NULL);
    ASSERT(strstr(script, "[ \"$mux_errors\" -gt 0 ]") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_start_session_creation_failure_aborts(void) {
    Arena a = arena_new();
    Project p;
    config_parse_string(&a, SIMPLE_CONFIG, strlen(SIMPLE_CONFIG), &p, NULL, 0);

    char *script = script_generate_start(&p);
    /* new-session failure must abort with a clear message instead of being
     * masked by read's here-string. */
    ASSERT(strstr(script, "mux_new_session() {\n  tmux new-session -d -P -F "
                          "'#{window_index} #{pane_index}'") != NULL);
    /* One silent retry for a server exiting after its last session was
     * killed; the retry's errors are shown and a real failure still aborts. */
    ASSERT(strstr(script, "if ! mux_session_info=$(mux_new_session 2>/dev/null) &&\n"
                          "  ! { sleep 0.2; mux_session_info=$(mux_new_session); }; then\n"
                          "  mux_abort") != NULL);
    ASSERT(strstr(script, "mux_abort 'failed to create tmux session "
                          "'\\''test'\\'''") != NULL);
    ASSERT(strstr(script, "read -r base_index pane_base_index <<< \"$mux_session_info\"") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_nonfatal_tmux_failures_are_recorded_not_fatal(void) {
    const char *cfg = "name: bad_layout\n"
                      "attach: false\n"
                      "windows:\n"
                      "  - one: echo x\n"
                      "  - two:\n"
                      "      layout: definitely-not-a-layout\n"
                      "      panes:\n"
                      "        - echo a\n"
                      "        - echo b\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char *script = script_generate_start(&p);
    const char *lay = strstr(script, "mux_try tmux select-layout -t ");
    const char *win1 = strstr(script, "mux_try tmux new-window -t ");
    const char *win2 = strstr(script, "mux_try tmux select-window -t ");
    ASSERT(lay != NULL);
    ASSERT(win1 != NULL);
    ASSERT(win2 != NULL);
    /* All windows are still emitted after the failing layout command. */
    ASSERT(lay < win2);
    (void)win1;
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_relative_root_resolved_against_invocation_cwd(void) {
    char orig_cwd[4096];
    ASSERT(getcwd(orig_cwd, sizeof(orig_cwd)) != NULL);

    char tmp[] = "/tmp/mux-root-rel-XXXXXX";
    ASSERT(mkdtemp(tmp) != NULL);
    ASSERT_EQ(0, chdir(tmp));
    /* mux resolves against getcwd(), which follows symlinks such as macOS's
     * /tmp -> /private/tmp, so expect the resolved path. */
    char dir[1024];
    ASSERT(getcwd(dir, sizeof(dir)) != NULL);

    const char *cfg = "name: app\n"
                      "root: app\n"
                      "windows:\n"
                      "  - one:\n"
                      "      root: sub\n"
                      "      panes:\n"
                      "        - pwd\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char expected_root[2048];
    snprintf(expected_root, sizeof(expected_root), "%s/app", dir);

    char *script = script_generate_start(&p);
    /* cd and tmux -c must land in the same directory: /cwd/app, and the
     * relative window root joins onto it (/cwd/app/sub), not /cwd/app/app. */
    {
        char expected_cd[4096];
        snprintf(expected_cd, sizeof(expected_cd), "cd %s\n", expected_root);
        ASSERT(strstr(script, expected_cd) != NULL);
    }
    {
        char expected_c[4096];
        snprintf(expected_c, sizeof(expected_c), "-c %s/sub", expected_root);
        ASSERT(strstr(script, expected_c) != NULL);
    }
    free(script);

    /* The stop script cd must resolve identically. */
    char *stop = script_generate_stop(&p);
    {
        char expected_cd[4096];
        snprintf(expected_cd, sizeof(expected_cd), "cd %s\n", expected_root);
        ASSERT(strstr(stop, expected_cd) != NULL);
    }
    free(stop);

    arena_free(&a);
    ASSERT_EQ(0, chdir(orig_cwd));
    rm_rf_tmpdir(dir);
    PASS();
}

TEST test_script_tilde_user_root_keeps_shell_expansion(void) {
    const char *cfg = "name: tuser\n"
                      "root: ~someuser/proj dir\n"
                      "windows:\n"
                      "  - one: echo x\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char *script = script_generate_start(&p);
    /* The ~user prefix stays unquoted so bash expands it at runtime. */
    ASSERT(strstr(script, "cd ~someuser/'proj dir'\n") != NULL);
    ASSERT(strstr(script, "-c ~someuser/'proj dir'") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

TEST test_script_herdr_backends_resolve_relative_root_absolutely(void) {
    char orig_cwd[4096];
    ASSERT(getcwd(orig_cwd, sizeof(orig_cwd)) != NULL);

    char tmp[] = "/tmp/mux-herdr-root-XXXXXX";
    ASSERT(mkdtemp(tmp) != NULL);
    ASSERT_EQ(0, chdir(tmp));
    /* mux resolves against getcwd(), which follows symlinks such as macOS's
     * /tmp -> /private/tmp, so expect the resolved path. */
    char dir[1024];
    ASSERT(getcwd(dir, sizeof(dir)) != NULL);

    const char *cfg = "name: herdrrel\n"
                      "root: app\n"
                      "attach: false\n"
                      "windows:\n"
                      "  - api:\n"
                      "      root: sub\n"
                      "      panes:\n"
                      "        - echo a\n"
                      "  - two: echo b\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char abs_root[1536], sub_root[1536], cd_line[2048];
    snprintf(abs_root, sizeof(abs_root), "%s/app", dir);
    snprintf(sub_root, sizeof(sub_root), "%s/app/sub", dir);
    snprintf(cd_line, sizeof(cd_line), "cd %s 2>/dev/null || true\n", abs_root);

    /* Herdr CLI backend: cd, workspace --cwd, tab --cwd and stop cd are all
     * absolute so the runtime `cd` cannot double-resolve the root. */
    char *herdr = script_generate_start_herdr(&p);
    ASSERT(strstr(herdr, cd_line) != NULL);
    {
        char expect[4096];
        snprintf(expect, sizeof(expect), "workspace create --focus --cwd %s --label herdrrel",
                 sub_root);
        ASSERT(strstr(herdr, expect) != NULL);
        snprintf(expect, sizeof(expect),
                 "tab create --workspace \"$workspace_id\" --no-focus --cwd %s --label two",
                 abs_root);
        ASSERT(strstr(herdr, expect) != NULL);
    }
    free(herdr);

    char *herdr_stop = script_generate_stop_herdr(&p);
    ASSERT(strstr(herdr_stop, cd_line) != NULL);
    free(herdr_stop);

    /* Herdr desktop backend: --folder and the cd sent into each terminal are
     * absolute (no app/app double resolution). */
    char *app = script_generate_start_herdr_app(&p);
    ASSERT(strstr(app, cd_line) != NULL);
    {
        char expect[4096];
        snprintf(expect, sizeof(expect),
                 "do space.new --machine '' --name herdrrel --tab api --folder %s", sub_root);
        ASSERT(strstr(app, expect) != NULL);
        snprintf(expect, sizeof(expect), "do tab.new --space \"$space_id\" --name two --folder %s",
                 abs_root);
        ASSERT(strstr(app, expect) != NULL);
        snprintf(expect, sizeof(expect), "mux_app_send \"$terminal_0_0\" %s 'echo a'", sub_root);
        ASSERT(strstr(app, expect) != NULL);
        snprintf(expect, sizeof(expect), "mux_app_send \"$terminal_1_0\" %s 'echo b'", abs_root);
        ASSERT(strstr(app, expect) != NULL);
    }
    free(app);

    char *app_stop = script_generate_stop_herdr_app(&p);
    ASSERT(strstr(app_stop, cd_line) != NULL);
    free(app_stop);

    arena_free(&a);
    ASSERT_EQ(0, chdir(orig_cwd));
    rm_rf_tmpdir(dir);
    PASS();
}

TEST test_script_explicitly_invalid_startup_pane_selects_pane_zero(void) {
    const char *cfg = "name: badpane\n"
                      "attach: false\n"
                      "startup_pane: -1\n"
                      "windows:\n"
                      "  - editor:\n"
                      "      focused_pane: 1\n"
                      "      panes:\n"
                      "        - echo a\n"
                      "        - echo b\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));
    ASSERT_EQ(-2, p.startup_pane);

    /* tmux: the startup pane selection must land on pane 0, not on the
     * focused_pane fallback. */
    char *script = script_generate_start(&p);
    const char *focus = strstr(script, "mux_try tmux select-pane -t =badpane:$((base_index+0))"
                                       ".$((pane_base_index+1))");
    const char *startup = strstr(script, "mux_try tmux select-pane -t =badpane:$((base_index+0))"
                                         ".$((pane_base_index+0))");
    ASSERT(focus != NULL);
    ASSERT(startup != NULL);
    ASSERT(focus < startup);
    free(script);

    /* herdr CLI backend: an explicitly invalid startup_pane focuses pane 0 of
     * the startup window (splits all carry --no-focus), not focused_pane 1. */
    char *herdr_script = script_generate_start_herdr(&p);
    ASSERT(strstr(herdr_script,
                  "pane split \"$pane_0_0\" --direction down --ratio 0.500000 --no-focus") != NULL);
    ASSERT(strstr(herdr_script, "--ratio 0.500000 --focus") == NULL);
    free(herdr_script);

    /* herdr-app: same fallback to pane 0 for the startup pane. */
    char *app_script = script_generate_start_herdr_app(&p);
    ASSERT(strstr(app_script, "do tab.select --id \"$tab_0\"") != NULL);
    ASSERT(strstr(app_script, "do pane.focus --pane \"$pane_0_0\"") != NULL);
    free(app_script);

    arena_free(&a);
    PASS();
}

TEST test_script_herdr_restart_and_detach_and_batched_json(void) {
    const char *cfg = "name: herdr_opts\n"
                      "attach: false\n"
                      "on_project_restart: echo restarted\n"
                      "windows:\n"
                      "  - main: echo hi\n";
    Arena a = arena_new();
    Project p;
    ASSERT_EQ(0, config_parse_string(&a, cfg, strlen(cfg), &p, NULL, 0));

    char *script = script_generate_start_herdr(&p);
    ASSERT(strstr(script, "echo restarted") != NULL);
    ASSERT(strstr(script, "\n  mux_herdr_attach\n") == NULL);
    ASSERT(strstr(script, "\nmux_herdr_attach\n") == NULL);
    ASSERT(strstr(script, "mux_herdr_json_values workspace_id tab_id pane_id") != NULL);
    free(script);
    arena_free(&a);
    PASS();
}

static char suite_cwd[4096];

static void restore_suite_cwd(void *udata) {
    (void)udata;
    if (suite_cwd[0] && chdir(suite_cwd) != 0) {
        perror("chdir");
    }
}

SUITE(script_suite) {
    GREATEST_SET_TEARDOWN_CB(restore_suite_cwd, NULL);
    RUN_TEST(test_script_start_contains_session);
    RUN_TEST(test_script_start_contains_windows);
    RUN_TEST(test_script_start_contains_attach);
    RUN_TEST(test_script_start_hooks);
    RUN_TEST(test_script_start_multi_pane);
    RUN_TEST(test_script_start_is_valid_bash);
    RUN_TEST(test_script_start_normalizes_empty_tmux_indices);
    RUN_TEST(test_script_start_tiles_after_splitting_panes);
    RUN_TEST(test_script_herdr_maps_windows_to_workspace_tabs_and_panes);
    RUN_TEST(test_script_herdr_reports_bad_json_without_python_traceback);
    RUN_TEST(test_script_herdr_starts_server_when_missing);
    RUN_TEST(test_script_herdr_stop_closes_matching_workspace);
    RUN_TEST(test_script_herdr_app_maps_windows_and_splits_before_commands);
    RUN_TEST(test_script_herdr_app_reuses_only_local_spaces);
    RUN_TEST(test_script_herdr_app_roots_hooks_and_startup_focus);
    RUN_TEST(test_script_herdr_app_stop_closes_named_space_without_core_restart);
    RUN_TEST(test_script_quotes_shell_sensitive_tmux_arguments);
    RUN_TEST(test_script_stop);
    RUN_TEST(test_script_stop_simple);
    RUN_TEST(test_script_stop_guards_kill_session_behind_has_session);
    RUN_TEST(test_script_stop_guards_custom_tmux_base);
    RUN_TEST(test_script_send_keys_escapes_trailing_semicolon);
    RUN_TEST(test_script_start_strict_mode_with_hook_isolation);
    RUN_TEST(test_script_start_session_creation_failure_aborts);
    RUN_TEST(test_script_nonfatal_tmux_failures_are_recorded_not_fatal);
    RUN_TEST(test_script_relative_root_resolved_against_invocation_cwd);
    RUN_TEST(test_script_herdr_backends_resolve_relative_root_absolutely);
    RUN_TEST(test_script_tilde_user_root_keeps_shell_expansion);
    RUN_TEST(test_script_explicitly_invalid_startup_pane_selects_pane_zero);
    RUN_TEST(test_script_sanitizes_dots_and_colons_in_session_name);
    RUN_TEST(test_script_nameless_window_omits_name_flag_and_uses_index);
    RUN_TEST(test_script_relative_window_root_and_tilde_space);
    RUN_TEST(test_script_out_of_range_focus_and_startup_pane_fall_back_to_zero);
    RUN_TEST(test_script_options_append_no_attach_and_no_pre_window);
    RUN_TEST(test_script_herdr_restart_and_detach_and_batched_json);
}

SUITE(script_fixture_suite) {
    RUN_TEST(test_script_detach_no_attach);
    RUN_TEST(test_script_wemux_command);
    RUN_TEST(test_script_socket_name);
    RUN_TEST(test_script_synchronize);
    RUN_TEST(test_script_startup_window_and_pane);
    RUN_TEST(test_script_pane_titles);
    RUN_TEST(test_script_focused_pane);
    RUN_TEST(test_script_window_root);
    RUN_TEST(test_script_hooks_all);
    RUN_TEST(test_script_sample_comprehensive);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    if (!getcwd(suite_cwd, sizeof(suite_cwd))) suite_cwd[0] = '\0';
    RUN_SUITE(script_suite);
    RUN_SUITE(script_fixture_suite);
    GREATEST_MAIN_END();
}
