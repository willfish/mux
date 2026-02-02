#include "greatest.h"
#include "arena.h"

#include <string.h>

TEST test_arena_new_free(void) {
    Arena a = arena_new();
    ASSERT(a.head != NULL);
    ASSERT(a.current != NULL);
    arena_free(&a);
    ASSERT(a.head == NULL);
    PASS();
}

TEST test_arena_alloc(void) {
    Arena a = arena_new();
    void *p1 = arena_alloc(&a, 64);
    ASSERT(p1 != NULL);
    void *p2 = arena_alloc(&a, 128);
    ASSERT(p2 != NULL);
    ASSERT(p1 != p2);
    arena_free(&a);
    PASS();
}

TEST test_arena_strdup(void) {
    Arena a = arena_new();
    char *s = arena_strdup(&a, "hello world");
    ASSERT_STR_EQ("hello world", s);

    char *s2 = arena_strdup(&a, "");
    ASSERT_STR_EQ("", s2);

    char *s3 = arena_strdup(&a, NULL);
    ASSERT(s3 == NULL);

    arena_free(&a);
    PASS();
}

TEST test_arena_strndup(void) {
    Arena a = arena_new();
    char *s = arena_strndup(&a, "hello world", 5);
    ASSERT_STR_EQ("hello", s);
    arena_free(&a);
    PASS();
}

TEST test_arena_alignment(void) {
    Arena a = arena_new();
    /* Allocate odd sizes, check alignment */
    void *p1 = arena_alloc(&a, 1);
    void *p2 = arena_alloc(&a, 1);
    ASSERT(((size_t)p1 % 8) == 0);
    ASSERT(((size_t)p2 % 8) == 0);
    arena_free(&a);
    PASS();
}

TEST test_arena_large_alloc(void) {
    Arena a = arena_new();
    /* Larger than default block size */
    void *p = arena_alloc(&a, ARENA_BLOCK_SIZE * 2);
    ASSERT(p != NULL);
    memset(p, 0xAB, ARENA_BLOCK_SIZE * 2);
    arena_free(&a);
    PASS();
}

TEST test_arena_reset(void) {
    Arena a = arena_new();
    arena_alloc(&a, 1024);
    arena_reset(&a);
    /* After reset, allocations should reuse space */
    void *p = arena_alloc(&a, 64);
    ASSERT(p != NULL);
    arena_free(&a);
    PASS();
}

TEST test_arena_many_allocs(void) {
    Arena a = arena_new();
    for (int i = 0; i < 10000; i++) {
        char *s = arena_strdup(&a, "test string allocation");
        ASSERT_STR_EQ("test string allocation", s);
    }
    arena_free(&a);
    PASS();
}

SUITE(arena_suite) {
    RUN_TEST(test_arena_new_free);
    RUN_TEST(test_arena_alloc);
    RUN_TEST(test_arena_strdup);
    RUN_TEST(test_arena_strndup);
    RUN_TEST(test_arena_alignment);
    RUN_TEST(test_arena_large_alloc);
    RUN_TEST(test_arena_reset);
    RUN_TEST(test_arena_many_allocs);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(arena_suite);
    GREATEST_MAIN_END();
}
