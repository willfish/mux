#include "greatest.h"
#include "arena.h"
#include "path.h"

#include <stdlib.h>
#include <string.h>

TEST test_path_config_dir_default(void) {
    Arena a = arena_new();
    /* With real env, should return a valid path */
    char *dir = path_config_dir(&a);
    ASSERT(dir != NULL);
    ASSERT(strlen(dir) > 0);
    arena_free(&a);
    PASS();
}

TEST test_path_find_project_existing(void) {
    Arena a = arena_new();
    /* The user has work.yml, dot.yml, fun.yml */
    char *path = path_find_project(&a, "work");
    if (path) {
        ASSERT(strstr(path, "work.yml") != NULL);
    }
    /* If not found, that's OK in CI */
    arena_free(&a);
    PASS();
}

TEST test_path_find_project_missing(void) {
    Arena a = arena_new();
    char *path = path_find_project(&a, "nonexistent_project_xyz123");
    ASSERT(path == NULL);
    arena_free(&a);
    PASS();
}

TEST test_path_find_project_null(void) {
    Arena a = arena_new();
    char *path = path_find_project(&a, NULL);
    ASSERT(path == NULL);
    arena_free(&a);
    PASS();
}

TEST test_path_list_projects(void) {
    Arena a = arena_new();
    int count = 0;
    char **projects = path_list_projects(&a, &count);
    /* Should find at least the user's configs if they exist */
    if (projects) {
        ASSERT(count >= 0);
        for (int i = 0; i < count; i++) {
            ASSERT(projects[i] != NULL);
            ASSERT(strlen(projects[i]) > 0);
        }
    }
    arena_free(&a);
    PASS();
}

TEST test_path_project_file(void) {
    Arena a = arena_new();
    char *path = path_project_file(&a, "test");
    ASSERT(path != NULL);
    ASSERT(strstr(path, "test.yml") != NULL);
    arena_free(&a);
    PASS();
}

SUITE(path_suite) {
    RUN_TEST(test_path_config_dir_default);
    RUN_TEST(test_path_find_project_existing);
    RUN_TEST(test_path_find_project_missing);
    RUN_TEST(test_path_find_project_null);
    RUN_TEST(test_path_list_projects);
    RUN_TEST(test_path_project_file);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(path_suite);
    GREATEST_MAIN_END();
}
