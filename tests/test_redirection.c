#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "redirection.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST(name) \
    g_tests_run++; \
    printf("Running %s... ", #name);

#define PASS() \
    g_tests_passed++; \
    printf("[PASS]\n");

void test_stdout_redirection(void) {
    TEST(test_stdout_redirection);
    const char *testfile = "/tmp/shellcraft_test_redir_out.txt";
    unlink(testfile);

    int saved[3];
    save_standard_fds(saved);

    Redirection *r = ast_create_redirection(REDIR_OUTPUT, testfile);
    int ret = apply_redirections(r);
    assert(ret == 0);

    /* Write directly to stdout */
    const char msg[] = "redirection test\n";
    ssize_t w = write(STDOUT_FILENO, msg, strlen(msg));
    (void)w;

    restore_standard_fds(saved);
    ast_free_redirection(r);

    /* Verify file content */
    FILE *fp = fopen(testfile, "r");
    assert(fp != NULL);
    char buf[64];
    char *res = fgets(buf, sizeof(buf), fp);
    assert(res != NULL);
    assert(strcmp(buf, "redirection test\n") == 0);
    fclose(fp);
    unlink(testfile);

    PASS();
}

void test_stdin_redirection(void) {
    TEST(test_stdin_redirection);
    const char *testfile = "/tmp/shellcraft_test_redir_in.txt";
    FILE *fp = fopen(testfile, "w");
    fprintf(fp, "input data\n");
    fclose(fp);

    int saved[3];
    save_standard_fds(saved);

    Redirection *r = ast_create_redirection(REDIR_INPUT, testfile);
    int ret = apply_redirections(r);
    assert(ret == 0);

    char buf[64];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    assert(n > 0);
    buf[n] = '\0';
    assert(strcmp(buf, "input data\n") == 0);

    restore_standard_fds(saved);
    ast_free_redirection(r);
    unlink(testfile);

    PASS();
}

int main(void) {
    printf("=== Redirection Unit Tests ===\n");
    test_stdout_redirection();
    test_stdin_redirection();
    printf("Result: %d/%d tests passed.\n\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
