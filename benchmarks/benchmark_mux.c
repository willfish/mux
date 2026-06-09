#include "arena.h"
#include "config.h"
#include "path.h"
#include "project.h"
#include "script.h"
#include "str.h"
#include "tmux.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const char *BENCH_CONFIG = "name: bench\n"
                                  "root: ~/projects/bench\n"
                                  "pre_window: nvm use 22\n"
                                  "windows:\n"
                                  "  - editor:\n"
                                  "      layout: main-vertical\n"
                                  "      panes:\n"
                                  "        - vim\n"
                                  "        - guard\n"
                                  "        - tail -f log/development.log\n"
                                  "  - server: bundle exec rails s\n"
                                  "  - console: bundle exec rails c\n";

static const char *TEMPLATE_CONFIG = "name: <%= @settings[\"name\"] %>\n"
                                     "root: <%= @settings[\"root\"] %>\n"
                                     "windows:\n"
                                     "  - server: echo <%= @settings[\"message\"] %>\n";

static volatile size_t bench_sink;

static long long now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(1);
    }
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void print_result(const char *name, int iterations, long long elapsed_ns) {
    double total_ms = (double)elapsed_ns / 1000000.0;
    double each_us = (double)elapsed_ns / (double)iterations / 1000.0;
    printf("%-28s %7d iterations %10.3f ms total %8.3f us/op\n", name, iterations, total_ms,
           each_us);
}

static void benchmark_parse_config(void) {
    const int iterations = 5000;
    long long start = now_ns();
    for (int i = 0; i < iterations; i++) {
        Arena a = arena_new();
        Project p;
        if (config_parse_string(&a, BENCH_CONFIG, strlen(BENCH_CONFIG), &p, NULL, 0) != 0) {
            fprintf(stderr, "benchmark: config_parse_string failed\n");
            exit(1);
        }
        bench_sink += (size_t)p.window_count;
        arena_free(&a);
    }
    print_result("parse config", iterations, now_ns() - start);
}

static void benchmark_template_config(void) {
    const int iterations = 5000;
    const char *settings[] = {"name", "templated", "root", "/tmp/templated", "message", "hello"};
    long long start = now_ns();
    for (int i = 0; i < iterations; i++) {
        Arena a = arena_new();
        Project p;
        if (config_parse_string(&a, TEMPLATE_CONFIG, strlen(TEMPLATE_CONFIG), &p, settings, 6) !=
            0) {
            fprintf(stderr, "benchmark: templated config_parse_string failed\n");
            exit(1);
        }
        bench_sink += strlen(p.name);
        arena_free(&a);
    }
    print_result("parse template config", iterations, now_ns() - start);
}

static void benchmark_script_generation(void) {
    const int iterations = 5000;
    long long start = now_ns();
    for (int i = 0; i < iterations; i++) {
        Arena a = arena_new();
        Project p;
        if (config_parse_string(&a, BENCH_CONFIG, strlen(BENCH_CONFIG), &p, NULL, 0) != 0) {
            fprintf(stderr, "benchmark: config_parse_string failed\n");
            exit(1);
        }
        char *script = script_generate_start(&p);
        if (!script) {
            fprintf(stderr, "benchmark: script_generate_start failed\n");
            exit(1);
        }
        bench_sink += strlen(script);
        free(script);
        arena_free(&a);
    }
    print_result("parse and generate script", iterations, now_ns() - start);
}

static void write_project_file(const char *dir, int index) {
    char path[512];
    snprintf(path, sizeof(path), "%s/project_%04d.yml", dir, index);
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "benchmark: cannot create %s: %s\n", path, strerror(errno));
        exit(1);
    }
    fprintf(f, "name: project_%04d\nwindows:\n  - main: echo hi\n", index);
    fclose(f);
}

static void cleanup_project_dir(const char *dir, int count) {
    for (int i = 0; i < count; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/project_%04d.yml", dir, i);
        unlink(path);
    }
    rmdir(dir);
}

static void benchmark_project_listing(void) {
    const int project_count = 1000;
    const int iterations = 100;
    char dir_template[] = "/tmp/mux-bench-XXXXXX";
    char *dir = mkdtemp(dir_template);
    if (!dir) {
        perror("mkdtemp");
        exit(1);
    }
    for (int i = 0; i < project_count; i++) {
        write_project_file(dir, i);
    }

    const char *previous = getenv("TMUXINATOR_CONFIG");
    char *previous_copy = previous ? strdup(previous) : NULL;
    setenv("TMUXINATOR_CONFIG", dir, 1);

    long long start = now_ns();
    for (int i = 0; i < iterations; i++) {
        Arena a = arena_new();
        int count = 0;
        char **projects = path_list_projects(&a, &count);
        if (!projects || count != project_count) {
            fprintf(stderr, "benchmark: expected %d projects, got %d\n", project_count, count);
            exit(1);
        }
        bench_sink += (size_t)count;
        arena_free(&a);
    }
    print_result("list 1000 projects", iterations, now_ns() - start);

    if (previous_copy) {
        setenv("TMUXINATOR_CONFIG", previous_copy, 1);
        free(previous_copy);
    } else {
        unsetenv("TMUXINATOR_CONFIG");
    }
    cleanup_project_dir(dir, project_count);
}

static void benchmark_active_session_filter(void) {
    const int project_count = 1000;
    const int active_count = 500;
    const int iterations = 100;
    Arena setup = arena_new();
    char **projects = arena_alloc(&setup, sizeof(char *) * (size_t)(project_count + 1));
    Str session_output = str_new();

    for (int i = 0; i < project_count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "project_%04d", i);
        projects[i] = arena_strdup(&setup, name);
        if (i < active_count) {
            str_appendf(&session_output, "%s\n", name);
        }
    }
    projects[project_count] = NULL;

    int session_count = 0;
    char **sessions = tmux_parse_session_names(&setup, str_cstr(&session_output), &session_count);
    long long start = now_ns();
    for (int i = 0; i < iterations; i++) {
        int matches = 0;
        for (int j = 0; j < project_count; j++) {
            if (tmux_session_names_contain(sessions, session_count, projects[j])) {
                matches++;
            }
        }
        if (matches != active_count) {
            fprintf(stderr, "benchmark: expected %d active matches, got %d\n", active_count,
                    matches);
            exit(1);
        }
        bench_sink += (size_t)matches;
    }
    print_result("filter active sessions", iterations, now_ns() - start);

    str_free(&session_output);
    arena_free(&setup);
}

int main(void) {
    benchmark_parse_config();
    benchmark_template_config();
    benchmark_script_generation();
    benchmark_project_listing();
    benchmark_active_session_filter();
    printf("benchmark sink: %zu\n", bench_sink);
    return 0;
}
