#include "greatest.h"
#include "str.h"

TEST test_str_new(void) {
    Str s = str_new();
    ASSERT_EQ(0, s.len);
    ASSERT(s.cap > 0);
    ASSERT_STR_EQ("", str_cstr(&s));
    str_free(&s);
    PASS();
}

TEST test_str_append(void) {
    Str s = str_new();
    str_append(&s, "hello");
    ASSERT_EQ(5, s.len);
    ASSERT_STR_EQ("hello", str_cstr(&s));

    str_append(&s, " world");
    ASSERT_EQ(11, s.len);
    ASSERT_STR_EQ("hello world", str_cstr(&s));

    str_free(&s);
    PASS();
}

TEST test_str_append_null(void) {
    Str s = str_new();
    str_append(&s, NULL);
    ASSERT_EQ(0, s.len);
    ASSERT_STR_EQ("", str_cstr(&s));
    str_free(&s);
    PASS();
}

TEST test_str_appendf(void) {
    Str s = str_new();
    str_appendf(&s, "count: %d", 42);
    ASSERT_STR_EQ("count: 42", str_cstr(&s));

    str_appendf(&s, " name: %s", "test");
    ASSERT_STR_EQ("count: 42 name: test", str_cstr(&s));

    str_free(&s);
    PASS();
}

TEST test_str_append_char(void) {
    Str s = str_new();
    str_append_char(&s, 'a');
    str_append_char(&s, 'b');
    str_append_char(&s, 'c');
    ASSERT_EQ(3, s.len);
    ASSERT_STR_EQ("abc", str_cstr(&s));
    str_free(&s);
    PASS();
}

TEST test_str_clear(void) {
    Str s = str_new();
    str_append(&s, "hello");
    ASSERT_EQ(5, s.len);
    str_clear(&s);
    ASSERT_EQ(0, s.len);
    ASSERT_STR_EQ("", str_cstr(&s));

    /* Can still append after clear */
    str_append(&s, "world");
    ASSERT_STR_EQ("world", str_cstr(&s));

    str_free(&s);
    PASS();
}

TEST test_str_with_capacity(void) {
    Str s = str_with_capacity(1024);
    ASSERT(s.cap >= 1024);
    ASSERT_EQ(0, s.len);
    str_append(&s, "test");
    ASSERT_STR_EQ("test", str_cstr(&s));
    str_free(&s);
    PASS();
}

TEST test_str_appendn(void) {
    Str s = str_new();
    str_appendn(&s, "hello world", 5);
    ASSERT_STR_EQ("hello", str_cstr(&s));
    ASSERT_EQ(5, s.len);
    str_free(&s);
    PASS();
}

TEST test_str_large(void) {
    Str s = str_new();
    for (int i = 0; i < 10000; i++) {
        str_append(&s, "a");
    }
    ASSERT_EQ(10000, s.len);
    str_free(&s);
    PASS();
}

TEST test_str_unicode(void) {
    Str s = str_new();
    str_append(&s, "héllo wörld 🌍");
    ASSERT(s.len > 0);
    ASSERT_STR_EQ("héllo wörld 🌍", str_cstr(&s));
    str_free(&s);
    PASS();
}

SUITE(str_suite) {
    RUN_TEST(test_str_new);
    RUN_TEST(test_str_append);
    RUN_TEST(test_str_append_null);
    RUN_TEST(test_str_appendf);
    RUN_TEST(test_str_append_char);
    RUN_TEST(test_str_clear);
    RUN_TEST(test_str_with_capacity);
    RUN_TEST(test_str_appendn);
    RUN_TEST(test_str_large);
    RUN_TEST(test_str_unicode);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(str_suite);
    GREATEST_MAIN_END();
}
