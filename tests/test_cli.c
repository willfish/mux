#include "greatest.h"
#include "cli.h"

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

TEST test_cli_copy(void) {
    char *argv[] = {"mux", "copy", "src", "dst"};
    CliArgs args;
    cli_parse(4, argv, &args);
    ASSERT_EQ(CMD_COPY, args.command);
    ASSERT_STR_EQ("src", args.project_name);
    ASSERT_STR_EQ("dst", args.copy_target);
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
    RUN_TEST(test_cli_copy);
    RUN_TEST(test_cli_list);
    RUN_TEST(test_cli_list_aliases);
    RUN_TEST(test_cli_doctor);
    RUN_TEST(test_cli_completions);
    RUN_TEST(test_cli_start_with_settings);
    RUN_TEST(test_cli_name_override);
    RUN_TEST(test_cli_append_flag);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(cli_suite);
    GREATEST_MAIN_END();
}
