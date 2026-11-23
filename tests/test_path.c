#include "arena.h"
#include "greatest.h"
#include "path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char *home;
    char *xdg;
    char *tc;
} SavedEnv;

static SavedEnv save_env(void) {
    SavedEnv s;
    const char *h = getenv("HOME");
    const char *x = getenv("XDG_CONFIG_HOME");
    const char *t = getenv("TMUXINATOR_CONFIG");
    s.home = h ? strdup(h) : NULL;
    s.xdg = x ? strdup(x) : NULL;
    s.tc = t ? strdup(t) : NULL;
    return s;
}

static void restore_env(SavedEnv *s) {
    if (s->home) {
        setenv("HOME", s->home, 1);
        free(s->home);
    } else {
        unsetenv("HOME");
    }
    if (s->xdg) {
        setenv("XDG_CONFIG_HOME", s->xdg, 1);
        free(s->xdg);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }
    if (s->tc) {
        setenv("TMUXINATOR_CONFIG", s->tc, 1);
        free(s->tc);
    } else {
        unsetenv("TMUXINATOR_CONFIG");
    }
}

static void write_dummy(const char *path) {
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "name: dummy\nwindows:\n  - main: echo hi\n");
        fclose(f);
    }
}

static void rm_rf(const char *dir) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    int rc = system(cmd);
    (void)rc;
}

TEST test_path_config_dir_precedence(void) {
    SavedEnv env = save_env();
    char tmp[] = "/tmp/mux-path-XXXXXX";
    char *base = mkdtemp(tmp);
    ASSERT(base != NULL);

    char home[256], xdg[256], xdg_tmux[512], cfg_tmux[512], dot_tmux[512], custom[512];
    snprintf(home, sizeof(home), "%s/home", base);
    snprintf(xdg, sizeof(xdg), "%s/xdg", base);
    snprintf(xdg_tmux, sizeof(xdg_tmux), "%s/tmuxinator", xdg);
    snprintf(cfg_tmux, sizeof(cfg_tmux), "%s/.config/tmuxinator", home);
    snprintf(dot_tmux, sizeof(dot_tmux), "%s/.tmuxinator", home);
    snprintf(custom, sizeof(custom), "%s/custom-nonexistent", base);

    setenv("HOME", home, 1);
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("TMUXINATOR_CONFIG");

    Arena a = arena_new();

    /* Default when nothing exists: ~/.config/tmuxinator */
    ASSERT_STR_EQ(cfg_tmux, path_config_dir(&a));

    /* Empty XDG_CONFIG_HOME must be treated as unset */
    setenv("XDG_CONFIG_HOME", "", 1);
    ASSERT_STR_EQ(cfg_tmux, path_config_dir(&a));

    /* Existing ~/.tmuxinator picked up if ~/.config/tmuxinator doesn't exist */
    ASSERT_EQ(0, path_mkdir_p(dot_tmux));
    ASSERT_STR_EQ(dot_tmux, path_config_dir(&a));

    /* Existing ~/.config/tmuxinator beats ~/.tmuxinator */
    ASSERT_EQ(0, path_mkdir_p(cfg_tmux));
    ASSERT_STR_EQ(cfg_tmux, path_config_dir(&a));

    /* Existing $XDG_CONFIG_HOME/tmuxinator beats ~/.config/tmuxinator */
    setenv("XDG_CONFIG_HOME", xdg, 1);
    ASSERT_EQ(0, path_mkdir_p(xdg_tmux));
    ASSERT_STR_EQ(xdg_tmux, path_config_dir(&a));

    /* $TMUXINATOR_CONFIG wins even if the directory doesn't exist yet */
    setenv("TMUXINATOR_CONFIG", custom, 1);
    ASSERT_STR_EQ(custom, path_config_dir(&a));

    arena_free(&a);
    restore_env(&env);
    rm_rf(base);
    PASS();
}

TEST test_path_multidir_yaml_subdirs_and_sorted_list(void) {
    SavedEnv env = save_env();
    char tmp[] = "/tmp/mux-path-list-XXXXXX";
    char *base = mkdtemp(tmp);
    ASSERT(base != NULL);

    char home[256], cfg_tmux[512], cfg_sub[768], dot_tmux[512], dir_yml[768], p1[768], p2[768],
        p3[1024], p4[768];
    snprintf(home, sizeof(home), "%s/home", base);
    snprintf(cfg_tmux, sizeof(cfg_tmux), "%s/.config/tmuxinator", home);
    snprintf(cfg_sub, sizeof(cfg_sub), "%s/team", cfg_tmux);
    snprintf(dot_tmux, sizeof(dot_tmux), "%s/.tmuxinator", home);
    snprintf(dir_yml, sizeof(dir_yml), "%s/not_a_file.yml", cfg_tmux);

    ASSERT_EQ(0, path_mkdir_p(cfg_sub));
    ASSERT_EQ(0, path_mkdir_p(dot_tmux));
    ASSERT_EQ(0, path_mkdir_p(dir_yml));

    snprintf(p1, sizeof(p1), "%s/zeta.yml", cfg_tmux);
    snprintf(p2, sizeof(p2), "%s/alpha.yaml", dot_tmux);
    snprintf(p3, sizeof(p3), "%s/service.yml", cfg_sub);
    snprintf(p4, sizeof(p4), "%s/zeta.yml", dot_tmux); /* duplicate in lower-priority dir */
    write_dummy(p1);
    write_dummy(p2);
    write_dummy(p3);
    write_dummy(p4);

    setenv("HOME", home, 1);
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("TMUXINATOR_CONFIG");

    Arena a = arena_new();

    /* Finds .yaml in fallback ~/.tmuxinator even when ~/.config/tmuxinator exists */
    char *found_alpha = path_find_project(&a, "alpha");
    ASSERT(found_alpha != NULL);
    ASSERT_STR_EQ(p2, found_alpha);

    /* Higher priority dir wins on duplicates */
    char *found_zeta = path_find_project(&a, "zeta");
    ASSERT(found_zeta != NULL);
    ASSERT_STR_EQ(p1, found_zeta);

    /* Finds nested project */
    char *found_nested = path_find_project(&a, "team/service");
    ASSERT(found_nested != NULL);
    ASSERT_STR_EQ(p3, found_nested);

    /* Missing and NULL */
    ASSERT(path_find_project(&a, "nonexistent_xyz") == NULL);
    ASSERT(path_find_project(&a, NULL) == NULL);

    /* Sorted, deduplicated, includes subdirs & .yaml, excludes directories named *.yml */
    int count = 0;
    char **projects = path_list_projects(&a, &count);
    ASSERT(projects != NULL);
    ASSERT_EQ(3, count);
    ASSERT_STR_EQ("alpha", projects[0]);
    ASSERT_STR_EQ("team/service", projects[1]);
    ASSERT_STR_EQ("zeta", projects[2]);
    ASSERT(projects[3] == NULL);

    arena_free(&a);
    restore_env(&env);
    rm_rf(base);
    PASS();
}

TEST test_path_find_local_yml_and_yaml(void) {
    char orig_cwd[4096];
    ASSERT(getcwd(orig_cwd, sizeof(orig_cwd)) != NULL);

    char tmp[] = "/tmp/mux-local-XXXXXX";
    char *dir = mkdtemp(tmp);
    ASSERT(dir != NULL);
    ASSERT_EQ(0, chdir(dir));

    Arena a = arena_new();
    ASSERT(path_find_local(&a) == NULL);

    /* Recognises .tmuxinator.yaml */
    write_dummy(".tmuxinator.yaml");
    char *local_yaml = path_find_local(&a);
    ASSERT(local_yaml != NULL);
    ASSERT(strstr(local_yaml, "/.tmuxinator.yaml") != NULL);

    /* .tmuxinator.yml takes precedence over .tmuxinator.yaml */
    write_dummy(".tmuxinator.yml");
    char *local_yml = path_find_local(&a);
    ASSERT(local_yml != NULL);
    ASSERT(strstr(local_yml, "/.tmuxinator.yml") != NULL);

    arena_free(&a);
    ASSERT_EQ(0, chdir(orig_cwd));
    rm_rf(dir);
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
    RUN_TEST(test_path_config_dir_precedence);
    RUN_TEST(test_path_multidir_yaml_subdirs_and_sorted_list);
    RUN_TEST(test_path_find_local_yml_and_yaml);
    RUN_TEST(test_path_project_file);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(path_suite);
    GREATEST_MAIN_END();
}
