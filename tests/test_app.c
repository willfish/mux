#include "app.h"
#include "greatest.h"
#include "path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int run_capture(int argc, char **argv, const char *stdin_text, char *out_buf,
                       size_t out_size) {
    char out_tmpl[] = "/tmp/mux-app-out-XXXXXX";
    int out_fd = mkstemp(out_tmpl);
    if (out_fd < 0) return -1;

    char in_tmpl[] = "/tmp/mux-app-in-XXXXXX";
    int in_fd = -1;
    int saved_in = -1;
    if (stdin_text) {
        in_fd = mkstemp(in_tmpl);
        if (in_fd < 0) {
            close(out_fd);
            unlink(out_tmpl);
            return -1;
        }
        size_t len = strlen(stdin_text);
        ssize_t w = write(in_fd, stdin_text, len);
        (void)w;
        lseek(in_fd, 0, SEEK_SET);
        saved_in = dup(STDIN_FILENO);
        dup2(in_fd, STDIN_FILENO);
    }

    fflush(stdout);
    int saved_out = dup(STDOUT_FILENO);
    dup2(out_fd, STDOUT_FILENO);

    int rc = mux_run(argc, argv);

    fflush(stdout);
    dup2(saved_out, STDOUT_FILENO);
    close(saved_out);

    if (saved_in >= 0) {
        dup2(saved_in, STDIN_FILENO);
        close(saved_in);
        clearerr(stdin);
        close(in_fd);
        unlink(in_tmpl);
    }

    lseek(out_fd, 0, SEEK_SET);
    ssize_t n = read(out_fd, out_buf, out_size - 1);
    if (n < 0) n = 0;
    out_buf[n] = '\0';
    close(out_fd);
    unlink(out_tmpl);

    return rc;
}

static void rm_rf(const char *dir) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    int rc = system(cmd);
    (void)rc;
}

TEST test_app_new_copy_list_delete_implode(void) {
    const char *prev_tc = getenv("TMUXINATOR_CONFIG");
    char *saved_tc = prev_tc ? strdup(prev_tc) : NULL;
    const char *prev_ed = getenv("EDITOR");
    char *saved_ed = prev_ed ? strdup(prev_ed) : NULL;

    char tmp[] = "/tmp/mux-app-dir-XXXXXX";
    char *base = mkdtemp(tmp);
    ASSERT(base != NULL);

    /* Non-existent nested $TMUXINATOR_CONFIG verifies recursive mkdir in cmd_new */
    char cfg_dir[512];
    snprintf(cfg_dir, sizeof(cfg_dir), "%s/nested/configs", base);
    setenv("TMUXINATOR_CONFIG", cfg_dir, 1);
    unsetenv("EDITOR");

    char out[8192];

    /* mux new alpha */
    char *argv_new1[] = {"mux", "new", "alpha"};
    ASSERT_EQ(0, run_capture(3, argv_new1, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "Created") != NULL);

    /* mux new team/beta (nested subdirectory project) */
    char *argv_new2[] = {"mux", "new", "team/beta"};
    ASSERT_EQ(0, run_capture(3, argv_new2, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "team/beta.yml") != NULL);

    /* mux copy alpha gamma */
    char *argv_cp[] = {"mux", "copy", "alpha", "gamma"};
    ASSERT_EQ(0, run_capture(4, argv_cp, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "Copied alpha to gamma") != NULL);

    /* mux list returns sorted projects including nested */
    char *argv_ls[] = {"mux", "list"};
    ASSERT_EQ(0, run_capture(2, argv_ls, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("alpha\ngamma\nteam/beta\n", out);

    /* mux delete alpha gamma (multiple targets in one command) */
    char *argv_del[] = {"mux", "delete", "alpha", "gamma"};
    ASSERT_EQ(0, run_capture(4, argv_del, "y\ny\n", out, sizeof(out)));
    ASSERT(strstr(out, "Deleted") != NULL);

    /* Only team/beta remains */
    ASSERT_EQ(0, run_capture(2, argv_ls, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("team/beta\n", out);

    /* mux implode removes remaining config */
    char *argv_imp[] = {"mux", "implode"};
    ASSERT_EQ(0, run_capture(2, argv_imp, "y\n", out, sizeof(out)));
    ASSERT_EQ(0, run_capture(2, argv_ls, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("", out);

    if (saved_tc) {
        setenv("TMUXINATOR_CONFIG", saved_tc, 1);
        free(saved_tc);
    } else {
        unsetenv("TMUXINATOR_CONFIG");
    }
    if (saved_ed) {
        setenv("EDITOR", saved_ed, 1);
        free(saved_ed);
    } else {
        unsetenv("EDITOR");
    }
    rm_rf(base);
    PASS();
}

TEST test_app_new_local_and_debug_local_fallback(void) {
    char orig_cwd[4096];
    ASSERT(getcwd(orig_cwd, sizeof(orig_cwd)) != NULL);
    const char *prev_ed = getenv("EDITOR");
    char *saved_ed = prev_ed ? strdup(prev_ed) : NULL;
    unsetenv("EDITOR");

    char tmp[] = "/tmp/mux-app-local-XXXXXX";
    char *dir = mkdtemp(tmp);
    ASSERT(dir != NULL);
    ASSERT_EQ(0, chdir(dir));

    char out[8192];
    char *argv_new[] = {"mux", "new", "--local", "localproj"};
    ASSERT_EQ(0, run_capture(4, argv_new, NULL, out, sizeof(out)));

    /* `mux debug --backend tmux` with no project arg picks up ./.tmuxinator.yml */
    char *argv_dbg[] = {"mux", "debug", "--backend", "tmux"};
    ASSERT_EQ(0, run_capture(4, argv_dbg, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "has-session -t =localproj") != NULL);

    ASSERT_EQ(0, chdir(orig_cwd));
    if (saved_ed) {
        setenv("EDITOR", saved_ed, 1);
        free(saved_ed);
    } else {
        unsetenv("EDITOR");
    }
    rm_rf(dir);
    PASS();
}

TEST test_app_debug_project_config_preserves_first_setting_and_args(void) {
    char tmp[] = "/tmp/mux-app-tpl-XXXXXX";
    char *dir = mkdtemp(tmp);
    ASSERT(dir != NULL);

    char cfg_path[512];
    snprintf(cfg_path, sizeof(cfg_path), "%s/tpl.yml", dir);
    FILE *f = fopen(cfg_path, "w");
    ASSERT(f != NULL);
    fputs("name: tpl\n"
          "root: <%= @settings['root'] %>\n"
          "windows:\n"
          "  - main: echo <%= @args[0] %> <%= @settings[\"port\"] %>\n",
          f);
    fclose(f);

    char out[8192];
    char *argv[] = {"mux",    "debug",         "--backend", "tmux",    "-p",
                    cfg_path, "root=/tmp/dir", "port=8080", "mybranch"};
    ASSERT_EQ(0, run_capture(9, argv, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "-c /tmp/dir") != NULL);
    ASSERT(strstr(out, "\"echo mybranch 8080\"") != NULL);

    rm_rf(dir);
    PASS();
}

TEST test_app_completions_valid_bash_zsh_fish(void) {
    char out[16384];

    char *argv_bash[] = {"mux", "completions", "bash"};
    ASSERT_EQ(0, run_capture(3, argv_bash, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "complete -F _mux mux") != NULL);
    ASSERT(strstr(out, "--backend") != NULL);

    char *argv_zsh[] = {"mux", "completions", "zsh"};
    ASSERT_EQ(0, run_capture(3, argv_zsh, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "compdef _mux mux") != NULL);

    char *argv_fish[] = {"mux", "completions", "fish"};
    ASSERT_EQ(0, run_capture(3, argv_fish, NULL, out, sizeof(out)));
    /* Verify space exists before -a (regression for missing-space syntax error) */
    ASSERT(strstr(out, "'-a ") == NULL);
    ASSERT(strstr(out, "' -a '(mux list 2>/dev/null)'") != NULL);

    PASS();
}

TEST test_app_version_and_doctor(void) {
    char out[4096];
    char *argv_ver[] = {"mux", "version"};
    ASSERT_EQ(0, run_capture(2, argv_ver, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("mux 0.2.0\n", out);

    char *argv_help[] = {"mux", "help"};
    ASSERT_EQ(0, run_capture(2, argv_help, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "Usage:") != NULL);

    char *argv_doc[] = {"mux", "doctor"};
    int doc_rc = run_capture(2, argv_doc, NULL, out, sizeof(out));
    ASSERT(doc_rc == 0 || doc_rc == 1);
    ASSERT(strstr(out, "Checking dependencies...") != NULL);
    ASSERT(strstr(out, "bash") != NULL);
    PASS();
}

TEST test_app_edit_local_start_stop_active_and_stop_all_with_mock_tmux(void) {
    const char *prev_tc = getenv("TMUXINATOR_CONFIG");
    char *saved_tc = prev_tc ? strdup(prev_tc) : NULL;
    const char *prev_path = getenv("PATH");
    char *saved_path = prev_path ? strdup(prev_path) : NULL;
    const char *prev_ed = getenv("EDITOR");
    char *saved_ed = prev_ed ? strdup(prev_ed) : NULL;
    const char *prev_backend = getenv("MUX_BACKEND");
    char *saved_backend = prev_backend ? strdup(prev_backend) : NULL;

    char orig_cwd[4096];
    ASSERT(getcwd(orig_cwd, sizeof(orig_cwd)) != NULL);

    char tmp[] = "/tmp/mux-app-full-XXXXXX";
    char *base = mkdtemp(tmp);
    ASSERT(base != NULL);

    char cfg_dir[512], bin_dir[512], mock_tmux[768], state_file[512], local_dir[512];
    snprintf(cfg_dir, sizeof(cfg_dir), "%s/configs", base);
    snprintf(bin_dir, sizeof(bin_dir), "%s/bin", base);
    snprintf(mock_tmux, sizeof(mock_tmux), "%s/tmux", bin_dir);
    snprintf(state_file, sizeof(state_file), "%s/sessions.txt", base);
    snprintf(local_dir, sizeof(local_dir), "%s/local", base);

    ASSERT_EQ(0, path_mkdir_p(cfg_dir));
    ASSERT_EQ(0, path_mkdir_p(bin_dir));
    ASSERT_EQ(0, path_mkdir_p(local_dir));

    /* Stateful mock tmux tracking active sessions in per-socket state files:
     * default socket uses $STATE, "-L/-S <name>" uses $STATE.<name> so tests
     * can prove project-specific server discovery (tmux_options: -L ...). */
    FILE *mf = fopen(mock_tmux, "w");
    ASSERT(mf != NULL);
    fprintf(mf,
            "#!/usr/bin/env bash\n"
            "STATE='%s'\n"
            "SOCKET=default\n"
            "while [ $# -gt 1 ]; do\n"
            "  case \"$1\" in\n"
            "    -L|-S) SOCKET=\"$2\"; shift 2 ;;\n"
            "    *) break ;;\n"
            "  esac\n"
            "done\n"
            "STATE_FILE=\"$STATE\"\n"
            "[ \"$SOCKET\" = default ] || STATE_FILE=\"$STATE.$SOCKET\"\n"
            "touch \"$STATE_FILE\"\n"
            "cmd=\"$1\"; shift || true\n"
            "case \"$cmd\" in\n"
            "  start-server) exit 0 ;;\n"
            "  list-sessions) cat \"$STATE_FILE\"; exit 0 ;;\n"
            "  has-session)\n"
            "    target=\"\"; while [ $# -gt 0 ]; do [ \"$1\" = -t ] && target=\"${2#=}\"; shift; "
            "done\n"
            "    grep -Fxq \"$target\" \"$STATE_FILE\" ;;\n"
            "  new-session)\n"
            "    if [ -n \"${MUX_MOCK_NEW_SESSION_FAIL:-}\" ]; then\n"
            "      echo \"mock new-session failure\" >&2\n"
            "      exit 1\n"
            "    fi\n"
            "    name=\"\"; while [ $# -gt 0 ]; do [ \"$1\" = -s ] && name=\"$2\"; shift; done\n"
            "    echo \"$name\" >> \"$STATE_FILE\"\n"
            "    echo '0 0' ;;\n"
            "  kill-session)\n"
            "    target=\"\"; while [ $# -gt 0 ]; do [ \"$1\" = -t ] && target=\"${2#=}\"; shift; "
            "done\n"
            "    grep -Fxv \"$target\" \"$STATE_FILE\" > \"${STATE_FILE}.tmp\" || true\n"
            "    mv \"${STATE_FILE}.tmp\" \"$STATE_FILE\" ;;\n"
            "  *) exit 0 ;;\n"
            "esac\n",
            state_file);
    fclose(mf);
    ASSERT_EQ(0, chmod(mock_tmux, 0755));

    char new_path[4096];
    snprintf(new_path, sizeof(new_path), "%s:%s", bin_dir,
             saved_path ? saved_path : "/usr/bin:/bin");
    setenv("PATH", new_path, 1);
    setenv("TMUXINATOR_CONFIG", cfg_dir, 1);
    setenv("MUX_BACKEND", "tmux", 1);
    setenv("EDITOR", "true", 1);

    char proj_yml[768];
    snprintf(proj_yml, sizeof(proj_yml), "%s/mycfg.yml", cfg_dir);
    FILE *pf = fopen(proj_yml, "w");
    ASSERT(pf != NULL);
    fputs("name: real.sess\nattach: false\nwindows:\n  - main: echo hi\n", pf);
    fclose(pf);

    char out[8192];

    /* mux edit works with EDITOR=true */
    char *argv_edit[] = {"mux", "edit", "mycfg"};
    ASSERT_EQ(0, run_capture(3, argv_edit, NULL, out, sizeof(out)));

    /* mux start & list --active (matches YAML name: real.sess -> real_sess) */
    char *argv_start[] = {"mux", "start", "mycfg"};
    ASSERT_EQ(0, run_capture(3, argv_start, NULL, out, sizeof(out)));

    char *argv_active[] = {"mux", "list", "--active"};
    ASSERT_EQ(0, run_capture(3, argv_active, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("mycfg\n", out);

    /* mux stop-all aborted on n, then confirmed on y */
    char *argv_sa[] = {"mux", "stop-all"};
    ASSERT_EQ(0, run_capture(2, argv_sa, "n\n", out, sizeof(out)));
    ASSERT(strstr(out, "Aborted.") != NULL);

    ASSERT_EQ(0, run_capture(2, argv_sa, "y\n", out, sizeof(out)));
    ASSERT_EQ(0, run_capture(3, argv_active, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("", out);

    /* mux stop-all when none active */
    ASSERT_EQ(0, run_capture(2, argv_sa, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "No active mux sessions.") != NULL);

    /* mux start / stop / local */
    ASSERT_EQ(0, run_capture(3, argv_start, NULL, out, sizeof(out)));
    char *argv_stop[] = {"mux", "stop", "mycfg"};
    ASSERT_EQ(0, run_capture(3, argv_stop, NULL, out, sizeof(out)));

    ASSERT_EQ(0, chdir(local_dir));
    char *argv_local[] = {"mux", "local"};
    ASSERT_EQ(1, run_capture(2, argv_local, NULL, out, sizeof(out)));

    FILE *lf = fopen(".tmuxinator.yml", "w");
    ASSERT(lf != NULL);
    fputs("name: loc\nattach: false\nwindows:\n  - main: echo ok\n", lf);
    fclose(lf);
    ASSERT_EQ(0, run_capture(2, argv_local, NULL, out, sizeof(out)));
    ASSERT_EQ(0, chdir(orig_cwd));

    /* Debug herdr, herdr-app, and unknown backend */
    char *argv_dh[] = {"mux", "debug", "--backend", "herdr", "mycfg"};
    ASSERT_EQ(0, run_capture(5, argv_dh, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "mux_herdr_ensure_server") != NULL);

    char *argv_dha[] = {"mux", "debug", "--backend", "herdr-app", "mycfg"};
    ASSERT_EQ(0, run_capture(5, argv_dha, NULL, out, sizeof(out)));
    ASSERT(strstr(out, "herdr_app_cmd=") != NULL);

    char *argv_dbad[] = {"mux", "debug", "--backend", "bogus", "mycfg"};
    ASSERT_EQ(1, run_capture(5, argv_dbad, NULL, out, sizeof(out)));

    /* Error paths: missing args, missing projects, unknown completion shell, delete abort */
    char *argv_e1[] = {"mux", "new"};
    ASSERT_EQ(1, run_capture(2, argv_e1, NULL, out, sizeof(out)));
    char *argv_e2[] = {"mux", "new", "mycfg"}; /* already exists */
    ASSERT_EQ(1, run_capture(3, argv_e2, NULL, out, sizeof(out)));
    char *argv_e3[] = {"mux", "edit"};
    ASSERT_EQ(1, run_capture(2, argv_e3, NULL, out, sizeof(out)));
    char *argv_e4[] = {"mux", "edit", "nope"};
    ASSERT_EQ(1, run_capture(3, argv_e4, NULL, out, sizeof(out)));
    char *argv_e5[] = {"mux", "copy", "onlyone"};
    ASSERT_EQ(1, run_capture(3, argv_e5, NULL, out, sizeof(out)));
    char *argv_e6[] = {"mux", "copy", "nope", "dst"};
    ASSERT_EQ(1, run_capture(4, argv_e6, NULL, out, sizeof(out)));
    char *argv_e7[] = {"mux", "copy", "mycfg", "mycfg"};
    ASSERT_EQ(1, run_capture(4, argv_e7, NULL, out, sizeof(out)));
    char *argv_e8[] = {"mux", "delete"};
    ASSERT_EQ(1, run_capture(2, argv_e8, NULL, out, sizeof(out)));
    char *argv_e9[] = {"mux", "delete", "nope"};
    ASSERT_EQ(1, run_capture(3, argv_e9, NULL, out, sizeof(out)));
    char *argv_e10[] = {"mux", "delete", "mycfg"};
    ASSERT_EQ(0, run_capture(3, argv_e10, "n\n", out, sizeof(out)));
    char *argv_e11[] = {"mux", "implode"};
    ASSERT_EQ(0, run_capture(2, argv_e11, "n\n", out, sizeof(out)));
    char *argv_e12[] = {"mux", "completions"};
    ASSERT_EQ(1, run_capture(2, argv_e12, NULL, out, sizeof(out)));
    char *argv_e13[] = {"mux", "completions", "csh"};
    ASSERT_EQ(1, run_capture(3, argv_e13, NULL, out, sizeof(out)));
    char *argv_e14[] = {"mux", "debug", "--unknown"};
    ASSERT_EQ(1, run_capture(3, argv_e14, NULL, out, sizeof(out)));

    if (saved_tc) {
        setenv("TMUXINATOR_CONFIG", saved_tc, 1);
        free(saved_tc);
    } else {
        unsetenv("TMUXINATOR_CONFIG");
    }
    if (saved_path) {
        setenv("PATH", saved_path, 1);
        free(saved_path);
    }
    if (saved_ed) {
        setenv("EDITOR", saved_ed, 1);
        free(saved_ed);
    } else {
        unsetenv("EDITOR");
    }
    if (saved_backend) {
        setenv("MUX_BACKEND", saved_backend, 1);
        free(saved_backend);
    } else {
        unsetenv("MUX_BACKEND");
    }
    rm_rf(base);
    PASS();
}

TEST test_app_tmux_options_socket_discovery_and_creation_failure(void) {
    const char *prev_tc = getenv("TMUXINATOR_CONFIG");
    char *saved_tc = prev_tc ? strdup(prev_tc) : NULL;
    const char *prev_path = getenv("PATH");
    char *saved_path = prev_path ? strdup(prev_path) : NULL;
    const char *prev_backend = getenv("MUX_BACKEND");
    char *saved_backend = prev_backend ? strdup(prev_backend) : NULL;

    char tmp[] = "/tmp/mux-app-sock-XXXXXX";
    char *base = mkdtemp(tmp);
    ASSERT(base != NULL);

    char cfg_dir[512], bin_dir[512], mock_tmux[768], state_file[512];
    snprintf(cfg_dir, sizeof(cfg_dir), "%s/configs", base);
    snprintf(bin_dir, sizeof(bin_dir), "%s/bin", base);
    snprintf(mock_tmux, sizeof(mock_tmux), "%s/tmux", bin_dir);
    snprintf(state_file, sizeof(state_file), "%s/sessions.txt", base);

    ASSERT_EQ(0, path_mkdir_p(cfg_dir));
    ASSERT_EQ(0, path_mkdir_p(bin_dir));

    /* Socket-aware mock: "-L <name>" selects a separate state file. */
    FILE *mf = fopen(mock_tmux, "w");
    ASSERT(mf != NULL);
    fprintf(mf,
            "#!/usr/bin/env bash\n"
            "STATE='%s'\n"
            "SOCKET=default\n"
            "while [ $# -gt 1 ]; do\n"
            "  case \"$1\" in\n"
            "    -L|-S) SOCKET=\"$2\"; shift 2 ;;\n"
            "    *) break ;;\n"
            "  esac\n"
            "done\n"
            "STATE_FILE=\"$STATE\"\n"
            "[ \"$SOCKET\" = default ] || STATE_FILE=\"$STATE.$SOCKET\"\n"
            "touch \"$STATE_FILE\"\n"
            "cmd=\"$1\"; shift || true\n"
            "case \"$cmd\" in\n"
            "  start-server) exit 0 ;;\n"
            "  list-sessions) cat \"$STATE_FILE\"; exit 0 ;;\n"
            "  has-session)\n"
            "    target=\"\"; while [ $# -gt 0 ]; do [ \"$1\" = -t ] && target=\"${2#=}\"; shift; "
            "done\n"
            "    grep -Fxq \"$target\" \"$STATE_FILE\" ;;\n"
            "  new-session)\n"
            "    if [ -n \"${MUX_MOCK_NEW_SESSION_FAIL:-}\" ]; then\n"
            "      echo \"mock new-session failure\" >&2\n"
            "      exit 1\n"
            "    fi\n"
            "    name=\"\"; while [ $# -gt 0 ]; do [ \"$1\" = -s ] && name=\"$2\"; shift; done\n"
            "    echo \"$name\" >> \"$STATE_FILE\"\n"
            "    echo '0 0' ;;\n"
            "  kill-session)\n"
            "    target=\"\"; while [ $# -gt 0 ]; do [ \"$1\" = -t ] && target=\"${2#=}\"; shift; "
            "done\n"
            "    grep -Fxv \"$target\" \"$STATE_FILE\" > \"${STATE_FILE}.tmp\" || true\n"
            "    mv \"${STATE_FILE}.tmp\" \"$STATE_FILE\" ;;\n"
            "  *) exit 0 ;;\n"
            "esac\n",
            state_file);
    fclose(mf);
    ASSERT_EQ(0, chmod(mock_tmux, 0755));

    char new_path[4096];
    snprintf(new_path, sizeof(new_path), "%s:%s", bin_dir,
             saved_path ? saved_path : "/usr/bin:/bin");
    setenv("PATH", new_path, 1);
    setenv("TMUXINATOR_CONFIG", cfg_dir, 1);
    setenv("MUX_BACKEND", "tmux", 1);

    /* The session only "exists" on the -L altsock server; the default
     * server's list-sessions is empty. list --active and stop-all must
     * discover it through tmux_options. */
    char alt_yml[768];
    snprintf(alt_yml, sizeof(alt_yml), "%s/altproj.yml", cfg_dir);
    FILE *af = fopen(alt_yml, "w");
    ASSERT(af != NULL);
    fputs("name: alt_sess\n"
          "attach: false\n"
          "tmux_options: -L altsock\n"
          "windows:\n"
          "  - main: echo hi\n",
          af);
    fclose(af);

    char alt_state[768];
    snprintf(alt_state, sizeof(alt_state), "%s.altsock", state_file);
    FILE *sf = fopen(alt_state, "w");
    ASSERT(sf != NULL);
    fputs("alt_sess\n", sf);
    fclose(sf);

    char out[8192];
    char *argv_active[] = {"mux", "list", "--active"};
    ASSERT_EQ(0, run_capture(3, argv_active, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("altproj\n", out);

    char *argv_sa[] = {"mux", "stop-all"};
    ASSERT_EQ(0, run_capture(2, argv_sa, "y\n", out, sizeof(out)));
    ASSERT(strstr(out, "Stop 1 active mux session(s)?") != NULL);
    ASSERT_EQ(0, run_capture(3, argv_active, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("", out);

    /* Session creation failure aborts the start script non-zero with a clear
     * message instead of being masked by read's here-string. */
    setenv("MUX_MOCK_NEW_SESSION_FAIL", "1", 1);
    char plain_yml[768];
    snprintf(plain_yml, sizeof(plain_yml), "%s/plainproj.yml", cfg_dir);
    FILE *pf = fopen(plain_yml, "w");
    ASSERT(pf != NULL);
    fputs("name: plain_sess\nattach: false\nwindows:\n  - main: echo hi\n", pf);
    fclose(pf);
    char *argv_start[] = {"mux", "start", "plainproj"};
    ASSERT_EQ(1, run_capture(3, argv_start, NULL, out, sizeof(out)));
    unsetenv("MUX_MOCK_NEW_SESSION_FAIL");

    if (saved_tc) {
        setenv("TMUXINATOR_CONFIG", saved_tc, 1);
        free(saved_tc);
    } else {
        unsetenv("TMUXINATOR_CONFIG");
    }
    if (saved_path) {
        setenv("PATH", saved_path, 1);
        free(saved_path);
    }
    if (saved_backend) {
        setenv("MUX_BACKEND", saved_backend, 1);
        free(saved_backend);
    } else {
        unsetenv("MUX_BACKEND");
    }
    rm_rf(base);
    PASS();
}

TEST test_app_implode_only_deletes_confirmed_directory(void) {
    const char *prev_tc = getenv("TMUXINATOR_CONFIG");
    char *saved_tc = prev_tc ? strdup(prev_tc) : NULL;
    const char *prev_home = getenv("HOME");
    char *saved_home = prev_home ? strdup(prev_home) : NULL;
    const char *prev_xdg = getenv("XDG_CONFIG_HOME");
    char *saved_xdg = prev_xdg ? strdup(prev_xdg) : NULL;

    char tmp[] = "/tmp/mux-app-imp-XXXXXX";
    char *base = mkdtemp(tmp);
    ASSERT(base != NULL);

    char xdg_dir[1024], xdg_root[512], home_dir[512], legacy_yml[1024], xdg_yml[4096];
    snprintf(xdg_root, sizeof(xdg_root), "%s/xdg", base);
    snprintf(xdg_dir, sizeof(xdg_dir), "%s/tmuxinator", xdg_root);
    snprintf(home_dir, sizeof(home_dir), "%s/home", base);
    snprintf(xdg_yml, sizeof(xdg_yml), "%s/only_xdg.yml", xdg_dir);
    snprintf(legacy_yml, sizeof(legacy_yml), "%s/home/.tmuxinator/legacy.yml", base);
    char legacy_dir[512];
    snprintf(legacy_dir, sizeof(legacy_dir), "%s/home/.tmuxinator", base);
    ASSERT_EQ(0, path_mkdir_p(xdg_dir));
    ASSERT_EQ(0, path_mkdir_p(legacy_dir));

    unsetenv("TMUXINATOR_CONFIG");
    setenv("HOME", home_dir, 1);
    setenv("XDG_CONFIG_HOME", xdg_root, 1);

    FILE *xf = fopen(xdg_yml, "w");
    ASSERT(xf != NULL);
    fputs("name: only_xdg\nwindows:\n  - a: echo\n", xf);
    fclose(xf);

    FILE *lf = fopen(legacy_yml, "w");
    ASSERT(lf != NULL);
    fputs("name: legacy\nwindows:\n  - a: echo\n", lf);
    fclose(lf);

    char out[8192];
    char *argv_ls[] = {"mux", "list"};
    ASSERT_EQ(0, run_capture(2, argv_ls, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("legacy\nonly_xdg\n", out);

    /* Implode prompts for the XDG directory and must delete only the configs
     * found there; ~/.tmuxinator/legacy.yml must survive. */
    char *argv_imp[] = {"mux", "implode"};
    ASSERT_EQ(0, run_capture(2, argv_imp, "y\n", out, sizeof(out)));
    ASSERT(strstr(out, "Deleted") != NULL);
    ASSERT(strstr(out, "only_xdg.yml") != NULL);
    ASSERT(strstr(out, "legacy.yml") == NULL);

    struct stat st;
    ASSERT_EQ(0, stat(legacy_yml, &st));
    ASSERT(stat(xdg_yml, &st) != 0);

    ASSERT_EQ(0, run_capture(2, argv_ls, NULL, out, sizeof(out)));
    ASSERT_STR_EQ("legacy\n", out);

    if (saved_tc) {
        setenv("TMUXINATOR_CONFIG", saved_tc, 1);
        free(saved_tc);
    } else {
        unsetenv("TMUXINATOR_CONFIG");
    }
    if (saved_home) {
        setenv("HOME", saved_home, 1);
        free(saved_home);
    } else {
        unsetenv("HOME");
    }
    if (saved_xdg) {
        setenv("XDG_CONFIG_HOME", saved_xdg, 1);
        free(saved_xdg);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }
    rm_rf(base);
    PASS();
}

SUITE(app_suite) {
    RUN_TEST(test_app_new_copy_list_delete_implode);
    RUN_TEST(test_app_new_local_and_debug_local_fallback);
    RUN_TEST(test_app_debug_project_config_preserves_first_setting_and_args);
    RUN_TEST(test_app_completions_valid_bash_zsh_fish);
    RUN_TEST(test_app_version_and_doctor);
    RUN_TEST(test_app_edit_local_start_stop_active_and_stop_all_with_mock_tmux);
    RUN_TEST(test_app_tmux_options_socket_discovery_and_creation_failure);
    RUN_TEST(test_app_implode_only_deletes_confirmed_directory);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(app_suite);
    GREATEST_MAIN_END();
}
