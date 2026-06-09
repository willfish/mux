#include "arena.h"
#include "greatest.h"
#include "tmux.h"

#include <string.h>

TEST test_tmux_has_session_command_escapes_session_name(void) {
    Arena a = arena_new();
    char *cmd = tmux_has_session_command(&a, "team's work; touch /tmp/mux-owned");
    ASSERT_STR_EQ("tmux has-session -t 'team'\"'\"'s work; touch /tmp/mux-owned' 2>/dev/null", cmd);
    arena_free(&a);
    PASS();
}

TEST test_tmux_parse_session_names(void) {
    Arena a = arena_new();
    int count = 0;
    char **sessions = tmux_parse_session_names(&a, "api\nweb\nworker\n", &count);
    ASSERT_EQ(3, count);
    ASSERT_STR_EQ("api", sessions[0]);
    ASSERT_STR_EQ("web", sessions[1]);
    ASSERT_STR_EQ("worker", sessions[2]);
    ASSERT(sessions[3] == NULL);
    arena_free(&a);
    PASS();
}

TEST test_tmux_session_names_contain_exact_match(void) {
    Arena a = arena_new();
    int count = 0;
    char **sessions = tmux_parse_session_names(&a, "app\napp-web\n", &count);
    ASSERT(tmux_session_names_contain(sessions, count, "app"));
    ASSERT(tmux_session_names_contain(sessions, count, "app-web"));
    ASSERT(!tmux_session_names_contain(sessions, count, "app-worker"));
    ASSERT(!tmux_session_names_contain(sessions, count, "ap"));
    arena_free(&a);
    PASS();
}

SUITE(tmux_suite) {
    RUN_TEST(test_tmux_has_session_command_escapes_session_name);
    RUN_TEST(test_tmux_parse_session_names);
    RUN_TEST(test_tmux_session_names_contain_exact_match);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(tmux_suite);
    GREATEST_MAIN_END();
}
