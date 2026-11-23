#include "arena.h"
#include "greatest.h"
#include "template.h"

#include <stdlib.h>

TEST test_template_no_tags(void) {
    Arena a = arena_new();
    const char *settings[] = {"foo", "bar"};
    char *r = template_substitute(&a, "hello world", settings, 2);
    ASSERT_STR_EQ("hello world", r);
    arena_free(&a);
    PASS();
}

TEST test_template_basic_substitution(void) {
    Arena a = arena_new();
    const char *settings[] = {"name", "myproject"};
    char *r = template_substitute(&a, "project: <%= @settings[\"name\"] %>", settings, 2);
    ASSERT_STR_EQ("project: myproject", r);
    arena_free(&a);
    PASS();
}

TEST test_template_multiple_substitutions(void) {
    Arena a = arena_new();
    const char *settings[] = {"host", "localhost", "port", "3000"};
    char *r = template_substitute(&a, "<%= @settings[\"host\"] %>:<%= @settings[\"port\"] %>",
                                  settings, 4);
    ASSERT_STR_EQ("localhost:3000", r);
    arena_free(&a);
    PASS();
}

TEST test_template_missing_key(void) {
    Arena a = arena_new();
    const char *settings[] = {"name", "test"};
    char *r = template_substitute(&a, "value: <%= @settings[\"missing\"] %>!", settings, 2);
    ASSERT_STR_EQ("value: !", r);
    arena_free(&a);
    PASS();
}

TEST test_template_no_settings(void) {
    Arena a = arena_new();
    char *r = template_substitute(&a, "plain text", NULL, 0);
    ASSERT_STR_EQ("plain text", r);
    arena_free(&a);
    PASS();
}

TEST test_template_null_input(void) {
    Arena a = arena_new();
    char *r = template_substitute(&a, NULL, NULL, 0);
    ASSERT(r == NULL);
    arena_free(&a);
    PASS();
}

TEST test_template_unclosed_tag(void) {
    Arena a = arena_new();
    const char *settings[] = {"k", "v"};
    char *r = template_substitute(&a, "prefix <%= @settings[\"k\"]", settings, 2);
    /* Unclosed tag should be left as-is */
    ASSERT_STR_EQ("prefix <%= @settings[\"k\"]", r);
    arena_free(&a);
    PASS();
}

TEST test_template_with_spaces_in_tag(void) {
    Arena a = arena_new();
    const char *settings[] = {"key", "value"};
    char *r = template_substitute(&a, "<%=  @settings[\"key\"]  %>", settings, 2);
    ASSERT_STR_EQ("value", r);
    arena_free(&a);
    PASS();
}

TEST test_template_parse_setting(void) {
    Arena a = arena_new();
    char *key = NULL, *value = NULL;
    int ret = template_parse_setting(&a, "host=localhost", &key, &value);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("host", key);
    ASSERT_STR_EQ("localhost", value);
    arena_free(&a);
    PASS();
}

TEST test_template_parse_setting_with_equals(void) {
    Arena a = arena_new();
    char *key = NULL, *value = NULL;
    int ret = template_parse_setting(&a, "expr=a=b", &key, &value);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("expr", key);
    ASSERT_STR_EQ("a=b", value);
    arena_free(&a);
    PASS();
}

TEST test_template_parse_setting_invalid(void) {
    Arena a = arena_new();
    char *key = NULL, *value = NULL;
    ASSERT_EQ(-1, template_parse_setting(&a, "noequals", &key, &value));
    ASSERT_EQ(-1, template_parse_setting(&a, "=value", &key, &value));
    arena_free(&a);
    PASS();
}

TEST test_template_single_quotes_and_tabs(void) {
    Arena a = arena_new();
    const char *settings[] = {"dir", "/srv/app"};
    char *r = template_substitute(&a, "root: <%=\t@settings['dir']\t%>", settings, 2);
    ASSERT_STR_EQ("root: /srv/app", r);
    arena_free(&a);
    PASS();
}

TEST test_template_unset_setting_with_no_cli_args(void) {
    Arena a = arena_new();
    char *r = template_substitute(&a, "prefix:<%= @settings[\"missing\"] %>:suffix", NULL, 0);
    ASSERT_STR_EQ("prefix::suffix", r);
    arena_free(&a);
    PASS();
}

TEST test_template_env_and_args(void) {
    Arena a = arena_new();
    setenv("MUX_TEST_ENV_VAR", "from_env", 1);
    const char *args[] = {"first", "second"};
    const char *tpl = "<%= ENV[\"MUX_TEST_ENV_VAR\"] %>|<%= ENV['MUX_TEST_ENV_VAR'] %>|"
                      "<%= @args[0] %>|<%= @args[1] %>|<%= @args[2] %>";
    char *r = template_substitute_ex(&a, tpl, strlen(tpl), NULL, 0, args, 2);
    ASSERT_STR_EQ("from_env|from_env|first|second|", r);
    unsetenv("MUX_TEST_ENV_VAR");
    arena_free(&a);
    PASS();
}

TEST test_template_respects_input_len(void) {
    Arena a = arena_new();
    const char *settings[] = {"k", "v"};
    const char *buf = "<%= @settings[\"k\"] %>TRAILING";
    char *r = template_substitute_ex(&a, buf, 21, settings, 2, NULL, 0);
    ASSERT_STR_EQ("v", r);
    arena_free(&a);
    PASS();
}

SUITE(template_suite) {
    RUN_TEST(test_template_no_tags);
    RUN_TEST(test_template_basic_substitution);
    RUN_TEST(test_template_multiple_substitutions);
    RUN_TEST(test_template_missing_key);
    RUN_TEST(test_template_no_settings);
    RUN_TEST(test_template_null_input);
    RUN_TEST(test_template_unclosed_tag);
    RUN_TEST(test_template_with_spaces_in_tag);
    RUN_TEST(test_template_parse_setting);
    RUN_TEST(test_template_parse_setting_with_equals);
    RUN_TEST(test_template_parse_setting_invalid);
    RUN_TEST(test_template_single_quotes_and_tabs);
    RUN_TEST(test_template_unset_setting_with_no_cli_args);
    RUN_TEST(test_template_env_and_args);
    RUN_TEST(test_template_respects_input_len);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(template_suite);
    GREATEST_MAIN_END();
}
