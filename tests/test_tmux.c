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

SUITE(tmux_suite) {
    RUN_TEST(test_tmux_has_session_command_escapes_session_name);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(tmux_suite);
    GREATEST_MAIN_END();
}
