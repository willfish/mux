#include "cli.h"
#include "greatest.h"

#include <string.h>

TEST test_cli_no_args(void) {
    char *argv[] = {"mux"};
    CliArgs args;
    cli_parse(1, argv, &args);
    ASSERT_EQ(CMD_HELP, args.command);
    PASS();
}

TEST test_cli_version(void) {
    char *argv[] = {"mux", "version"};
    CliArgs args;
    cli_parse(2, argv, &args);
    ASSERT_EQ(CMD_VERSION, args.command);
    PASS();
}

TEST test_cli_version_flag(void) {
    char *argv[] = {"mux", "--version"};
    CliArgs args;
    cli_parse(2, argv, &args);
    ASSERT_EQ(CMD_VERSION, args.command);
    PASS();
}

TEST test_cli_help(void) {
    char *argv[] = {"mux", "help"};
    CliArgs args;
    cli_parse(2, argv, &args);
    ASSERT_EQ(CMD_HELP, args.command);
    PASS();
}

TEST test_cli_help_flag(void) {
    char *argv[] = {"mux", "--help"};
    CliArgs args;
    cli_parse(2, argv, &args);
    ASSERT_EQ(CMD_HELP, args.command);
    PASS();
}

TEST test_cli_start(void) {
    char *argv[] = {"mux", "start", "work"};
    CliArgs args;
    cli_parse(3, argv, &args);
    ASSERT_EQ(CMD_START, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_start_shortcut(void) {
    char *argv[] = {"mux", "s", "work"};
    CliArgs args;
    cli_parse(3, argv, &args);
    ASSERT_EQ(CMD_START, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_implicit_start(void) {
    /* Unknown command treated as project name for start */
    char *argv[] = {"mux", "work"};
    CliArgs args;
    cli_parse(2, argv, &args);
    ASSERT_EQ(CMD_START, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_stop(void) {
    char *argv[] = {"mux", "stop", "work"};
    CliArgs args;
    cli_parse(3, argv, &args);
    ASSERT_EQ(CMD_STOP, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_debug(void) {
    char *argv[] = {"mux", "debug", "work"};
    CliArgs args;
    cli_parse(3, argv, &args);
    ASSERT_EQ(CMD_DEBUG, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_new(void) {
    char *argv[] = {"mux", "new", "myproject"};
    CliArgs args;
    cli_parse(3, argv, &args);
    ASSERT_EQ(CMD_NEW, args.command);
    ASSERT_STR_EQ("myproject", args.project_name);
    PASS();
}

TEST test_cli_edit(void) {
    char *argv[] = {"mux", "edit", "work"};
    CliArgs args;
    cli_parse(3, argv, &args);
    ASSERT_EQ(CMD_EDIT, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_edit_aliases(void) {
    char *argv1[] = {"mux", "e", "work"};
    CliArgs args;
    cli_parse(3, argv1, &args);
    ASSERT_EQ(CMD_EDIT, args.command);
    ASSERT_STR_EQ("work", args.project_name);

    char *argv2[] = {"mux", "open", "work"};
    cli_parse(3, argv2, &args);
    ASSERT_EQ(CMD_EDIT, args.command);
    ASSERT_STR_EQ("work", args.project_name);

    char *argv3[] = {"mux", "o", "work"};
    cli_parse(3, argv3, &args);
    ASSERT_EQ(CMD_EDIT, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_copy(void) {
    char *argv[] = {"mux", "copy", "src", "dst"};
    CliArgs args;
    cli_parse(4, argv, &args);
    ASSERT_EQ(CMD_COPY, args.command);
    ASSERT_STR_EQ("src", args.project_name);
    ASSERT_STR_EQ("dst", args.copy_target);
    PASS();
}

TEST test_cli_copy_cp_alias(void) {
    char *argv[] = {"mux", "cp", "src", "dst"};
    CliArgs args;
    cli_parse(4, argv, &args);
    ASSERT_EQ(CMD_COPY, args.command);
    ASSERT_STR_EQ("src", args.project_name);
    ASSERT_STR_EQ("dst", args.copy_target);
    PASS();
}

TEST test_cli_delete_rm_alias(void) {
    char *argv[] = {"mux", "rm", "work"};
    CliArgs args;
    cli_parse(3, argv, &args);
    ASSERT_EQ(CMD_DELETE, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_implode_alias(void) {
    char *argv[] = {"mux", "i"};
    CliArgs args;
    cli_parse(2, argv, &args);
    ASSERT_EQ(CMD_IMPLODE, args.command);
    PASS();
}

TEST test_cli_list(void) {
    char *argv[] = {"mux", "list"};
    CliArgs args;
    cli_parse(2, argv, &args);
    ASSERT_EQ(CMD_LIST, args.command);
    PASS();
}

TEST test_cli_list_aliases(void) {
    char *argv1[] = {"mux", "ls"};
    CliArgs args;
    cli_parse(2, argv1, &args);
    ASSERT_EQ(CMD_LIST, args.command);

    char *argv2[] = {"mux", "l"};
    cli_parse(2, argv2, &args);
    ASSERT_EQ(CMD_LIST, args.command);
    PASS();
}

TEST test_cli_doctor(void) {
    char *argv[] = {"mux", "doctor"};
    CliArgs args;
    cli_parse(2, argv, &args);
    ASSERT_EQ(CMD_DOCTOR, args.command);
    PASS();
}

TEST test_cli_completions(void) {
    char *argv[] = {"mux", "completions", "bash"};
    CliArgs args;
    cli_parse(3, argv, &args);
    ASSERT_EQ(CMD_COMPLETIONS, args.command);
    ASSERT_STR_EQ("bash", args.completion_shell);
    PASS();
}

TEST test_cli_start_with_settings(void) {
    char *argv[] = {"mux", "work", "host=localhost", "port=3000"};
    CliArgs args;
    cli_parse(4, argv, &args);
    ASSERT_EQ(CMD_START, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    ASSERT_EQ(2, args.setting_count);
    PASS();
}

TEST test_cli_name_override(void) {
    char *argv[] = {"mux", "start", "--name", "custom", "work"};
    CliArgs args;
    cli_parse(5, argv, &args);
    ASSERT_EQ(CMD_START, args.command);
    ASSERT_STR_EQ("custom", args.override_name);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_append_flag(void) {
    char *argv[] = {"mux", "start", "--append", "work"};
    CliArgs args;
    cli_parse(4, argv, &args);
    ASSERT_EQ(CMD_START, args.command);
    ASSERT(args.append);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_backend_flag(void) {
    char *argv[] = {"mux", "start", "--backend", "herdr", "work"};
    CliArgs args;
    cli_parse(5, argv, &args);
    ASSERT_EQ(CMD_START, args.command);
    ASSERT_STR_EQ("herdr", args.backend);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_herdr_app_backend_flag(void) {
    char *argv[] = {"mux", "start", "--backend", "herdr-app", "work"};
    CliArgs args;
    ASSERT_EQ(0, cli_parse(5, argv, &args));
    ASSERT_EQ(CMD_START, args.command);
    ASSERT_STR_EQ("herdr-app", args.backend);
    ASSERT_STR_EQ("work", args.project_name);
    PASS();
}

TEST test_cli_flags_after_project_and_implicit_start(void) {
    char *argv1[] = {"mux", "debug", "work", "-n", "custom", "--no-pre-window"};
    CliArgs args;
    ASSERT_EQ(0, cli_parse(6, argv1, &args));
    ASSERT_EQ(CMD_DEBUG, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    ASSERT_STR_EQ("custom", args.override_name);
    ASSERT(args.no_pre_window);

    char *argv2[] = {"mux", "work", "--no-attach", "-n", "alt", "host=localhost"};
    ASSERT_EQ(0, cli_parse(6, argv2, &args));
    ASSERT_EQ(CMD_START, args.command);
    ASSERT_STR_EQ("work", args.project_name);
    ASSERT_STR_EQ("alt", args.override_name);
    ASSERT_EQ(0, args.attach_override);
    ASSERT_EQ(1, args.setting_count);
    ASSERT_STR_EQ("host=localhost", args.settings[0]);
    PASS();
}

TEST test_cli_unknown_flag_and_missing_arg_fail(void) {
    char *argv1[] = {"mux", "start", "--unknown-flag", "work"};
    CliArgs args;
    ASSERT_EQ(-1, cli_parse(4, argv1, &args));

    char *argv2[] = {"mux", "start", "--name"};
    ASSERT_EQ(-1, cli_parse(3, argv2, &args));

    char *argv3[] = {"mux", "start", "--attach=maybe", "work"};
    ASSERT_EQ(-1, cli_parse(4, argv3, &args));
    PASS();
}

TEST test_cli_local_and_project_config_keep_first_setting(void) {
    char *argv1[] = {"mux", "local", "root=/tmp/a", "port=3000"};
    CliArgs args;
    ASSERT_EQ(0, cli_parse(4, argv1, &args));
    ASSERT_EQ(CMD_LOCAL, args.command);
    ASSERT(args.project_name == NULL);
    ASSERT_EQ(2, args.setting_count);
    ASSERT_STR_EQ("root=/tmp/a", args.settings[0]);
    ASSERT_STR_EQ("port=3000", args.settings[1]);

    char *argv2[] = {"mux", "debug", "-p", "template.yml", "root=/tmp/b", "host=localhost"};
    ASSERT_EQ(0, cli_parse(6, argv2, &args));
    ASSERT_EQ(CMD_DEBUG, args.command);
    ASSERT_STR_EQ("template.yml", args.project_config);
    ASSERT(args.project_name == NULL);
    ASSERT_EQ(2, args.setting_count);
    ASSERT_STR_EQ("root=/tmp/b", args.settings[0]);
    ASSERT_STR_EQ("host=localhost", args.settings[1]);
    PASS();
}

TEST test_cli_delete_multiple_and_new_local_and_attach_flags(void) {
    char *argv1[] = {"mux", "delete", "alpha", "beta", "gamma"};
    CliArgs args;
    ASSERT_EQ(0, cli_parse(5, argv1, &args));
    ASSERT_EQ(CMD_DELETE, args.command);
    ASSERT_EQ(3, args.delete_count);
    ASSERT_STR_EQ("alpha", args.delete_targets[0]);
    ASSERT_STR_EQ("beta", args.delete_targets[1]);
    ASSERT_STR_EQ("gamma", args.delete_targets[2]);

    char *argv2[] = {"mux", "new", "--local"};
    ASSERT_EQ(0, cli_parse(3, argv2, &args));
    ASSERT_EQ(CMD_NEW, args.command);
    ASSERT(args.local_flag);

    char *argv3[] = {"mux", "start", "--attach=false", "work"};
    ASSERT_EQ(0, cli_parse(4, argv3, &args));
    ASSERT_EQ(0, args.attach_override);

    char *argv4[] = {"mux", "list", "-A"};
    ASSERT_EQ(0, cli_parse(3, argv4, &args));
    ASSERT_EQ(CMD_LIST, args.command);
    ASSERT(args.active_only);
    PASS();
}

TEST test_cli_inline_equals_double_dash_and_aliases(void) {
    CliArgs args;

    char *argv1[] = {
        "mux", "--backend=herdr-app", "--name=alt", "--project-config=foo.yml", "--attach=yes",
        "--",  "--literal-arg"};
    ASSERT_EQ(0, cli_parse(7, argv1, &args));
    ASSERT_EQ(CMD_START, args.command);
    ASSERT_STR_EQ("herdr-app", args.backend);
    ASSERT_STR_EQ("alt", args.override_name);
    ASSERT_STR_EQ("foo.yml", args.project_config);
    ASSERT_EQ(1, args.attach_override);
    ASSERT_EQ(1, args.setting_count);
    ASSERT_STR_EQ("--literal-arg", args.settings[0]);

    char *argv2[] = {"mux", "stop-all"};
    ASSERT_EQ(0, cli_parse(2, argv2, &args));
    ASSERT_EQ(CMD_STOP_ALL, args.command);

    char *argv3[] = {"mux", "start", "--help"};
    ASSERT_EQ(0, cli_parse(3, argv3, &args));
    ASSERT_EQ(CMD_HELP, args.command);

    char *argv4[] = {"mux", "start", "--version"};
    ASSERT_EQ(0, cli_parse(3, argv4, &args));
    ASSERT_EQ(CMD_VERSION, args.command);

    char *argv5[] = {"mux", "start", "--attach", "work"};
    ASSERT_EQ(0, cli_parse(4, argv5, &args));
    ASSERT_EQ(1, args.attach_override);

    char *argv6[] = {"mux", "n", "proj"};
    ASSERT_EQ(0, cli_parse(3, argv6, &args));
    ASSERT_EQ(CMD_NEW, args.command);

    char *argv7[] = {"mux", "v"};
    ASSERT_EQ(0, cli_parse(2, argv7, &args));
    ASSERT_EQ(CMD_VERSION, args.command);

    char *argv8[] = {"mux", "h"};
    ASSERT_EQ(0, cli_parse(2, argv8, &args));
    ASSERT_EQ(CMD_HELP, args.command);

    char *argv9[] = {"mux", "start", "-b"};
    ASSERT_EQ(-1, cli_parse(3, argv9, &args));

    char *argv10[] = {"mux", "start", "-p"};
    ASSERT_EQ(-1, cli_parse(3, argv10, &args));

    PASS();
}

SUITE(cli_suite) {
    RUN_TEST(test_cli_no_args);
    RUN_TEST(test_cli_version);
    RUN_TEST(test_cli_version_flag);
    RUN_TEST(test_cli_help);
    RUN_TEST(test_cli_help_flag);
    RUN_TEST(test_cli_start);
    RUN_TEST(test_cli_start_shortcut);
    RUN_TEST(test_cli_implicit_start);
    RUN_TEST(test_cli_stop);
    RUN_TEST(test_cli_debug);
    RUN_TEST(test_cli_new);
    RUN_TEST(test_cli_edit);
    RUN_TEST(test_cli_edit_aliases);
    RUN_TEST(test_cli_copy);
    RUN_TEST(test_cli_copy_cp_alias);
    RUN_TEST(test_cli_delete_rm_alias);
    RUN_TEST(test_cli_implode_alias);
    RUN_TEST(test_cli_list);
    RUN_TEST(test_cli_list_aliases);
    RUN_TEST(test_cli_doctor);
    RUN_TEST(test_cli_completions);
    RUN_TEST(test_cli_start_with_settings);
    RUN_TEST(test_cli_name_override);
    RUN_TEST(test_cli_append_flag);
    RUN_TEST(test_cli_backend_flag);
    RUN_TEST(test_cli_herdr_app_backend_flag);
    RUN_TEST(test_cli_flags_after_project_and_implicit_start);
    RUN_TEST(test_cli_unknown_flag_and_missing_arg_fail);
    RUN_TEST(test_cli_local_and_project_config_keep_first_setting);
    RUN_TEST(test_cli_delete_multiple_and_new_local_and_attach_flags);
    RUN_TEST(test_cli_inline_equals_double_dash_and_aliases);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(cli_suite);
    GREATEST_MAIN_END();
}
