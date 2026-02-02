#include "greatest.h"
#include "arena.h"
#include "shell.h"

TEST test_shell_escape_simple(void) {
    Arena a = arena_new();
    char *r = shell_escape(&a, "hello");
    ASSERT_STR_EQ("'hello'", r);
    arena_free(&a);
    PASS();
}

TEST test_shell_escape_spaces(void) {
    Arena a = arena_new();
    char *r = shell_escape(&a, "hello world");
    ASSERT_STR_EQ("'hello world'", r);
    arena_free(&a);
    PASS();
}

TEST test_shell_escape_single_quote(void) {
    Arena a = arena_new();
    char *r = shell_escape(&a, "it's");
    ASSERT_STR_EQ("'it'\"'\"'s'", r);
    arena_free(&a);
    PASS();
}

TEST test_shell_escape_empty(void) {
    Arena a = arena_new();
    char *r = shell_escape(&a, "");
    ASSERT_STR_EQ("''", r);
    arena_free(&a);
    PASS();
}

TEST test_shell_escape_null(void) {
    Arena a = arena_new();
    char *r = shell_escape(&a, NULL);
    ASSERT_STR_EQ("''", r);
    arena_free(&a);
    PASS();
}

TEST test_shell_escape_semicolon(void) {
    Arena a = arena_new();
    char *r = shell_escape(&a, "cmd; rm -rf /");
    ASSERT_STR_EQ("'cmd; rm -rf /'", r);
    arena_free(&a);
    PASS();
}

TEST test_shell_escape_double_quotes(void) {
    Arena a = arena_new();
    char *r = shell_escape(&a, "say \"hello\"");
    ASSERT_STR_EQ("'say \"hello\"'", r);
    arena_free(&a);
    PASS();
}

TEST test_shell_escape_unicode(void) {
    Arena a = arena_new();
    char *r = shell_escape(&a, "📀 emoji");
    ASSERT_STR_EQ("'📀 emoji'", r);
    arena_free(&a);
    PASS();
}

TEST test_path_expand_tilde(void) {
    Arena a = arena_new();
    const char *home = getenv("HOME");
    if (!home) {
        arena_free(&a);
        SKIP();
    }

    char *r = path_expand(&a, "~/projects");
    /* Should start with $HOME */
    ASSERT(strncmp(r, home, strlen(home)) == 0);
    /* Should end with /projects */
    ASSERT(strstr(r, "/projects") != NULL);
    arena_free(&a);
    PASS();
}

TEST test_path_expand_tilde_only(void) {
    Arena a = arena_new();
    const char *home = getenv("HOME");
    if (!home) {
        arena_free(&a);
        SKIP();
    }
    char *r = path_expand(&a, "~");
    ASSERT_STR_EQ(home, r);
    arena_free(&a);
    PASS();
}

TEST test_path_expand_no_tilde(void) {
    Arena a = arena_new();
    char *r = path_expand(&a, "/usr/local/bin");
    ASSERT_STR_EQ("/usr/local/bin", r);
    arena_free(&a);
    PASS();
}

TEST test_path_expand_null(void) {
    Arena a = arena_new();
    char *r = path_expand(&a, NULL);
    ASSERT(r == NULL);
    arena_free(&a);
    PASS();
}

SUITE(shell_suite) {
    RUN_TEST(test_shell_escape_simple);
    RUN_TEST(test_shell_escape_spaces);
    RUN_TEST(test_shell_escape_single_quote);
    RUN_TEST(test_shell_escape_empty);
    RUN_TEST(test_shell_escape_null);
    RUN_TEST(test_shell_escape_semicolon);
    RUN_TEST(test_shell_escape_double_quotes);
    RUN_TEST(test_shell_escape_unicode);
    RUN_TEST(test_path_expand_tilde);
    RUN_TEST(test_path_expand_tilde_only);
    RUN_TEST(test_path_expand_no_tilde);
    RUN_TEST(test_path_expand_null);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(shell_suite);
    GREATEST_MAIN_END();
}
