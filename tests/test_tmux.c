#include "arena.h"
#include "greatest.h"
#include "project.h"
#include "tmux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

TEST test_tmux_sanitize_session_name(void) {
    Arena a = arena_new();
    ASSERT_STR_EQ("my_proj", tmux_sanitize_session_name(&a, "my.proj"));
    ASSERT_STR_EQ("a_b_c", tmux_sanitize_session_name(&a, "a:b.c"));
    ASSERT_STR_EQ("plain", tmux_sanitize_session_name(&a, "plain"));
    ASSERT_STR_EQ("", tmux_sanitize_session_name(&a, NULL));
    arena_free(&a);
    PASS();
}

TEST test_tmux_has_session_command_escapes_session_name(void) {
    Arena a = arena_new();
    char *cmd = tmux_has_session_command(&a, "team's work; touch /tmp/mux-owned");
    ASSERT_STR_EQ("tmux has-session -t '=team'\"'\"'s work; touch /tmp/mux-owned' 2>/dev/null",
                  cmd);
    arena_free(&a);
    PASS();
}

TEST test_tmux_has_session_command_sanitizes_dots_and_colons(void) {
    Arena a = arena_new();
    char *cmd = tmux_has_session_command(&a, "my.app:1");
    ASSERT_STR_EQ("tmux has-session -t '=my_app_1' 2>/dev/null", cmd);
    arena_free(&a);
    PASS();
}

TEST test_tmux_parse_session_names(void) {
    Arena a = arena_new();
    int count = 0;
    char **sessions = tmux_parse_session_names(&a, "worker\napi\n\nweb", &count);
    ASSERT_EQ(3, count);
    /* Sorted alphabetically for O(log N) lookup */
    ASSERT_STR_EQ("api", sessions[0]);
    ASSERT_STR_EQ("web", sessions[1]);
    ASSERT_STR_EQ("worker", sessions[2]);
    ASSERT(sessions[3] == NULL);
    arena_free(&a);
    PASS();
}

TEST test_tmux_parse_session_names_empty_and_null(void) {
    Arena a = arena_new();
    int count = -1;
    char **s1 = tmux_parse_session_names(&a, NULL, &count);
    ASSERT_EQ(0, count);
    ASSERT(s1[0] == NULL);

    char **s2 = tmux_parse_session_names(&a, "", &count);
    ASSERT_EQ(0, count);
    ASSERT(s2[0] == NULL);
    arena_free(&a);
    PASS();
}

TEST test_tmux_session_names_contain_exact_match(void) {
    Arena a = arena_new();
    int count = 0;
    char **sessions = tmux_parse_session_names(&a, "app-web\napp\nzeta\n", &count);
    ASSERT(tmux_session_names_contain(sessions, count, "app"));
    ASSERT(tmux_session_names_contain(sessions, count, "app-web"));
    ASSERT(tmux_session_names_contain(sessions, count, "zeta"));
    ASSERT(!tmux_session_names_contain(sessions, count, "app-worker"));
    ASSERT(!tmux_session_names_contain(sessions, count, "ap"));
    ASSERT(!tmux_session_names_contain(sessions, 0, "app"));
    ASSERT(!tmux_session_names_contain(NULL, count, "app"));
    arena_free(&a);
    PASS();
}

TEST test_tmux_list_and_has_session_with_mock_binary(void) {
    const char *orig_path = getenv("PATH");
    char *saved_path = orig_path ? strdup(orig_path) : NULL;

    char tmp[] = "/tmp/mux-mock-tmux-XXXXXX";
    char *dir = mkdtemp(tmp);
    ASSERT(dir != NULL);

    char mock_path[512];
    snprintf(mock_path, sizeof(mock_path), "%s/tmux", dir);
    FILE *f = fopen(mock_path, "w");
    ASSERT(f != NULL);
    fputs("#!/usr/bin/env bash\n"
          "if [ \"$1\" = \"list-sessions\" ]; then\n"
          "  printf 'web\\napi\\n'\n"
          "  exit 0\n"
          "fi\n"
          "for arg in \"$@\"; do\n"
          "  if [ \"$arg\" = \"=my_proj\" ] || [ \"$arg\" = \"=api\" ]; then\n"
          "    exit 0\n"
          "  fi\n"
          "done\n"
          "exit 1\n",
          f);
    fclose(f);
    ASSERT_EQ(0, chmod(mock_path, 0755));

    char new_path[4096];
    snprintf(new_path, sizeof(new_path), "%s:%s", dir, saved_path ? saved_path : "/usr/bin:/bin");
    setenv("PATH", new_path, 1);

    Arena a = arena_new();
    int count = 0;
    char **sessions = tmux_list_sessions(&a, &count);
    ASSERT_EQ(2, count);
    ASSERT_STR_EQ("api", sessions[0]);
    ASSERT_STR_EQ("web", sessions[1]);

    ASSERT_EQ(1, tmux_has_session(&a, "api"));
    ASSERT_EQ(0, tmux_has_session(&a, "ap"));

    Project p;
    project_init(&p);
    p.name = "my.proj";
    p.socket_name = "sock";
    p.socket_path = "/tmp/s.sock";
    p.tmux_options = "-2";
    p.tmux_command = mock_path;
    ASSERT_EQ(1, tmux_project_has_session(&a, &p));

    p.name = "missing";
    ASSERT_EQ(0, tmux_project_has_session(&a, &p));
    ASSERT_EQ(0, tmux_project_has_session(&a, NULL));

    arena_free(&a);
    if (saved_path) {
        setenv("PATH", saved_path, 1);
        free(saved_path);
    }
    unlink(mock_path);
    rmdir(dir);
    PASS();
}

SUITE(tmux_suite) {
    RUN_TEST(test_tmux_sanitize_session_name);
    RUN_TEST(test_tmux_has_session_command_escapes_session_name);
    RUN_TEST(test_tmux_has_session_command_sanitizes_dots_and_colons);
    RUN_TEST(test_tmux_parse_session_names);
    RUN_TEST(test_tmux_parse_session_names_empty_and_null);
    RUN_TEST(test_tmux_session_names_contain_exact_match);
    RUN_TEST(test_tmux_list_and_has_session_with_mock_binary);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(tmux_suite);
    GREATEST_MAIN_END();
}
