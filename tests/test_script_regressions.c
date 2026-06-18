#include "arena.h"
#include "config.h"
#include "greatest.h"
#include "script.h"
#include "str.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE_PATH "tests/fixtures/"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    char *content = malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    size_t nread = fread(content, 1, (size_t)size, f);
    fclose(f);
    content[nread] = '\0';
    return content;
}

static char *normalize_script_commands(const char *script) {
    Str normalized = str_new();
    const char *line = script;

    while (*line) {
        const char *line_end = strchr(line, '\n');
        size_t len = line_end ? (size_t)(line_end - line) : strlen(line);
        const char *start = line;
        while (len > 0 && (*start == ' ' || *start == '\t')) {
            start++;
            len--;
        }
        while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) {
            len--;
        }

        if (len > 0 && start[0] != '#') {
            str_appendn(&normalized, start, len);
            str_append_char(&normalized, '\n');
        }

        if (!line_end) break;
        line = line_end + 1;
    }

    char *result = strdup(str_cstr(&normalized));
    str_free(&normalized);
    return result;
}

static enum greatest_test_res assert_script_fixture_matches(const char *fixture_name) {
    Arena a = arena_new();
    Str config_path = str_new();
    Str expected_path = str_new();
    str_appendf(&config_path, "%s%s.yml", FIXTURE_PATH, fixture_name);
    str_appendf(&expected_path, "%s%s.commands", FIXTURE_PATH, fixture_name);

    Project p;
    ASSERT_EQ(0, config_parse(&a, str_cstr(&config_path), &p, NULL, 0));
    char *script = script_generate_start(&p);
    ASSERT(script != NULL);
    char *actual = normalize_script_commands(script);
    char *expected = read_file(str_cstr(&expected_path));
    ASSERT(expected != NULL);
    ASSERT_STR_EQ(expected, actual);

    free(expected);
    free(actual);
    free(script);
    str_free(&expected_path);
    str_free(&config_path);
    arena_free(&a);
    PASS();
}

TEST test_script_regression_sample(void) {
    return assert_script_fixture_matches("sample");
}

TEST test_script_regression_hooks(void) {
    return assert_script_fixture_matches("hooks");
}

TEST test_script_regression_pane_titles(void) {
    return assert_script_fixture_matches("pane_titles");
}

TEST test_script_regression_window_root(void) {
    return assert_script_fixture_matches("window_root");
}

TEST test_script_regression_focused_pane(void) {
    return assert_script_fixture_matches("focused_pane");
}

TEST test_script_regression_sample_deprecations(void) {
    return assert_script_fixture_matches("sample_deprecations");
}

SUITE(script_regression_suite) {
    RUN_TEST(test_script_regression_sample);
    RUN_TEST(test_script_regression_hooks);
    RUN_TEST(test_script_regression_pane_titles);
    RUN_TEST(test_script_regression_window_root);
    RUN_TEST(test_script_regression_focused_pane);
    RUN_TEST(test_script_regression_sample_deprecations);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(script_regression_suite);
    GREATEST_MAIN_END();
}
