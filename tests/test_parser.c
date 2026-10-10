#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "parser.h"
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

void test_parse_simple_command(void) {
    TEST(test_parse_simple_command);
    TokenList tl;
    char err[128];
    tokenize_line("ls -la /tmp", &tl, err, sizeof(err));
    AstProgram *prog = parse_tokens(&tl, err, sizeof(err));
    assert(prog != NULL);
    assert(prog->conditionals != NULL);
    AstPipeline *pipe = prog->conditionals->pipeline;
    assert(pipe != NULL);
    assert(pipe->count == 1);
    assert(pipe->commands[0]->argc == 3);
    assert(strcmp(pipe->commands[0]->argv[0], "ls") == 0);
    assert(strcmp(pipe->commands[0]->argv[1], "-la") == 0);
    assert(strcmp(pipe->commands[0]->argv[2], "/tmp") == 0);
    ast_free_program(prog);
    token_list_free(&tl);
    PASS();
}

void test_parse_multi_stage_pipeline(void) {
    TEST(test_parse_multi_stage_pipeline);
    TokenList tl;
    char err[128];
    tokenize_line("cat file.txt | grep error | sort | uniq -c > summary.txt", &tl, err, sizeof(err));
    AstProgram *prog = parse_tokens(&tl, err, sizeof(err));
    assert(prog != NULL);
    AstPipeline *pipe = prog->conditionals->pipeline;
    assert(pipe->count == 4);
    assert(strcmp(pipe->commands[0]->argv[0], "cat") == 0);
    assert(strcmp(pipe->commands[1]->argv[0], "grep") == 0);
    assert(strcmp(pipe->commands[2]->argv[0], "sort") == 0);
    assert(strcmp(pipe->commands[3]->argv[0], "uniq") == 0);
    assert(pipe->commands[3]->redirections != NULL);
    assert(pipe->commands[3]->redirections->type == REDIR_OUTPUT);
    assert(strcmp(pipe->commands[3]->redirections->target, "summary.txt") == 0);
    ast_free_program(prog);
    token_list_free(&tl);
    PASS();
}

void test_parse_conditionals(void) {
    TEST(test_parse_conditionals);
    TokenList tl;
    char err[128];
    tokenize_line("mkdir test_dir && cd test_dir || echo 'failed'", &tl, err, sizeof(err));
    AstProgram *prog = parse_tokens(&tl, err, sizeof(err));
    assert(prog != NULL);
    AstConditional *c1 = prog->conditionals;
    assert(c1 != NULL);
    assert(c1->op_to_next == COND_AND);
    assert(c1->next != NULL);
    assert(c1->next->op_to_next == COND_OR);
    assert(c1->next->next != NULL);
    ast_free_program(prog);
    token_list_free(&tl);
    PASS();
}

void test_parse_syntax_error(void) {
    TEST(test_parse_syntax_error);
    TokenList tl;
    char err[128];
    tokenize_line("cat | | wc", &tl, err, sizeof(err));
    AstProgram *prog = parse_tokens(&tl, err, sizeof(err));
    assert(prog == NULL);
    assert(strstr(err, "syntax error") != NULL);
    token_list_free(&tl);
    PASS();
}

int main(void) {
    printf("=== Parser Unit Tests ===\n");
    test_parse_simple_command();
    test_parse_multi_stage_pipeline();
    test_parse_conditionals();
    test_parse_syntax_error();
    printf("Result: %d/%d tests passed.\n\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
