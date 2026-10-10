#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST(name) \
    g_tests_run++; \
    printf("Running %s... ", #name);

#define PASS() \
    g_tests_passed++; \
    printf("[PASS]\n");

void test_simple_words(void) {
    TEST(test_simple_words);
    TokenList tl;
    char err[64];
    bool ok = tokenize_line("echo hello world", &tl, err, sizeof(err));
    assert(ok);
    assert(tl.count == 4); /* echo, hello, world, EOF */
    assert(tl.tokens[0].type == TOKEN_WORD && strcmp(tl.tokens[0].value, "echo") == 0);
    assert(tl.tokens[1].type == TOKEN_WORD && strcmp(tl.tokens[1].value, "hello") == 0);
    assert(tl.tokens[2].type == TOKEN_WORD && strcmp(tl.tokens[2].value, "world") == 0);
    assert(tl.tokens[3].type == TOKEN_EOF);
    token_list_free(&tl);
    PASS();
}

void test_operators(void) {
    TEST(test_operators);
    TokenList tl;
    char err[64];
    bool ok = tokenize_line("cat file | grep text && ls -l || false ; exit &", &tl, err, sizeof(err));
    assert(ok);
    assert(tl.tokens[2].type == TOKEN_PIPE);
    assert(tl.tokens[5].type == TOKEN_AND);
    assert(tl.tokens[8].type == TOKEN_OR);
    assert(tl.tokens[10].type == TOKEN_SEMICOLON);
    assert(tl.tokens[12].type == TOKEN_BACKGROUND);
    token_list_free(&tl);
    PASS();
}

void test_redirection_tokens(void) {
    TEST(test_redirection_tokens);
    TokenList tl;
    char err[64];
    bool ok = tokenize_line("cmd < in.txt > out.txt >> app.txt 2> err.txt 2>> err_app.txt 2>&1", &tl, err, sizeof(err));
    assert(ok);
    assert(tl.tokens[1].type == TOKEN_REDIRECT_IN);
    assert(tl.tokens[3].type == TOKEN_REDIRECT_OUT);
    assert(tl.tokens[5].type == TOKEN_REDIRECT_APPEND);
    assert(tl.tokens[7].type == TOKEN_REDIRECT_STDERR);
    assert(tl.tokens[9].type == TOKEN_REDIRECT_STDERR_APPEND);
    assert(tl.tokens[11].type == TOKEN_REDIRECT_STDERR_TO_STDOUT);
    token_list_free(&tl);
    PASS();
}

void test_quotes_and_escapes(void) {
    TEST(test_quotes_and_escapes);
    TokenList tl;
    char err[64];
    bool ok = tokenize_line("echo 'single quoted' \"double quoted\" hello\\ world", &tl, err, sizeof(err));
    assert(ok);
    assert(strcmp(tl.tokens[1].value, "single quoted") == 0);
    assert(tl.tokens[1].single_quoted == true);
    assert(strcmp(tl.tokens[2].value, "double quoted") == 0);
    assert(tl.tokens[2].double_quoted == true);
    assert(strcmp(tl.tokens[3].value, "hello world") == 0);
    token_list_free(&tl);
    PASS();
}

void test_unmatched_quotes(void) {
    TEST(test_unmatched_quotes);
    TokenList tl;
    char err[64];
    bool ok = tokenize_line("echo 'unclosed quote", &tl, err, sizeof(err));
    assert(!ok);
    assert(strstr(err, "unmatched single quote") != NULL);
    token_list_free(&tl);
    PASS();
}

int main(void) {
    printf("=== Tokenizer Unit Tests ===\n");
    test_simple_words();
    test_operators();
    test_redirection_tokens();
    test_quotes_and_escapes();
    test_unmatched_quotes();
    printf("Result: %d/%d tests passed.\n\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
