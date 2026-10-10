#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "builtins.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST(name) \
    g_tests_run++; \
    printf("Running %s... ", #name);

#define PASS() \
    g_tests_passed++; \
    printf("[PASS]\n");

void test_builtin_cd_and_pwd(void) {
    TEST(test_builtin_cd_and_pwd);
    ShellState shell;
    memset(&shell, 0, sizeof(ShellState));

    char orig[PATH_MAX];
    getcwd(orig, sizeof(orig));

    char *argv_cd[] = {"cd", "/tmp", NULL};
    int ret = builtin_cd(2, argv_cd, &shell);
    assert(ret == 0);

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    assert(strcmp(cwd, "/tmp") == 0);

    /* Return to original */
    chdir(orig);
    PASS();
}

void test_builtin_export(void) {
    TEST(test_builtin_export);
    char *argv_exp[] = {"export", "MY_TEST_KEY=12345", NULL};
    int ret = builtin_export(2, argv_exp);
    assert(ret == 0);
    const char *val = getenv("MY_TEST_KEY");
    assert(val != NULL && strcmp(val, "12345") == 0);
    PASS();
}

void test_alias_system(void) {
    TEST(test_alias_system);
    alias_init();

    char *argv_al[] = {"alias", "ll=ls -la", NULL};
    int ret = builtin_alias(2, argv_al);
    assert(ret == 0);

    const char *resolved = alias_lookup("ll");
    assert(resolved != NULL && strcmp(resolved, "ls -la") == 0);

    char *argv_unal[] = {"unalias", "ll", NULL};
    ret = builtin_unalias(2, argv_unal);
    assert(ret == 0);
    assert(alias_lookup("ll") == NULL);

    alias_cleanup();
    PASS();
}

void test_builtin_type(void) {
    TEST(test_builtin_type);
    assert(is_builtin("cd"));
    assert(is_builtin("exit"));
    assert(is_builtin("pwd"));
    assert(!is_builtin("nonexistent_command_xyz"));
    PASS();
}

int main(void) {
    printf("=== Builtins Unit Tests ===\n");
    test_builtin_cd_and_pwd();
    test_builtin_export();
    test_alias_system();
    test_builtin_type();
    printf("Result: %d/%d tests passed.\n\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
