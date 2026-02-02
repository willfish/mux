#include "greatest.h"
#include "arena.h"
#include "template.h"

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
    char *r = template_substitute(
        &a, "<%= @settings[\"host\"] %>:<%= @settings[\"port\"] %>", settings, 4);
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
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(template_suite);
    GREATEST_MAIN_END();
}
