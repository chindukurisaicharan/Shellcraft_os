#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "expansion.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST(name) \
    g_tests_run++; \
    printf("Running %s... ", #name);

#define PASS() \
    g_tests_passed++; \
    printf("[PASS]\n");

void test_env_var_expansion(void) {
    TEST(test_env_var_expansion);
    setenv("SHELLCRAFT_USER", "antigravity", 1);
    char *res1 = expand_variables_and_tilde("$SHELLCRAFT_USER", 0, getpid());
    assert(res1 != NULL && strcmp(res1, "antigravity") == 0);
    free(res1);

    char *res2 = expand_variables_and_tilde("User: ${SHELLCRAFT_USER}_dev", 0, getpid());
    assert(res2 != NULL && strcmp(res2, "User: antigravity_dev") == 0);
    free(res2);
    PASS();
}

void test_special_vars(void) {
    TEST(test_special_vars);
    char *res1 = expand_variables_and_tilde("status=$?", 42, 1234);
    assert(res1 != NULL && strcmp(res1, "status=42") == 0);
    free(res1);

    char *res2 = expand_variables_and_tilde("pid=$$", 0, 1234);
    assert(res2 != NULL && strcmp(res2, "pid=1234") == 0);
    free(res2);
    PASS();
}

void test_tilde_expansion(void) {
    TEST(test_tilde_expansion);
    const char *home = getenv("HOME");
    if (home) {
        char *res1 = expand_variables_and_tilde("~", 0, getpid());
        assert(res1 != NULL && strcmp(res1, home) == 0);
        free(res1);

        char expected[512];
        snprintf(expected, sizeof(expected), "%s/projects", home);
        char *res2 = expand_variables_and_tilde("~/projects", 0, getpid());
        assert(res2 != NULL && strcmp(res2, expected) == 0);
        free(res2);
    }
    PASS();
}

void test_command_substitution(void) {
    TEST(test_command_substitution);
    ShellState shell;
    shell_init(&shell, 0, NULL);
    shell.is_interactive = false;

    char *res = expand_command_substitutions("echo $(printf 'hello')", &shell);
    assert(res != NULL && strstr(res, "hello") != NULL);
    free(res);
    shell_cleanup(&shell);
    PASS();
}

int main(void) {
    printf("=== Expansion Unit Tests ===\n");
    test_env_var_expansion();
    test_special_vars();
    test_tilde_expansion();
    test_command_substitution();
    printf("Result: %d/%d tests passed.\n\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
