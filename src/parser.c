#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "parser.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static const Token *peek(Parser *p) {
    if (p->pos < p->tokens->count) {
        return &p->tokens->tokens[p->pos];
    }
    return NULL;
}

static const Token *advance(Parser *p) {
    if (p->pos < p->tokens->count) {
        return &p->tokens->tokens[p->pos++];
    }
    return NULL;
}

static bool match(Parser *p, TokenType type) {
    const Token *tok = peek(p);
    if (tok && tok->type == type) {
        advance(p);
        return true;
    }
    return false;
}

static void set_error(Parser *p, const char *fmt, ...) {
    p->has_error = true;
    if (p->error_msg && p->error_size > 0) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(p->error_msg, p->error_size, fmt, args);
        va_end(args);
    }
}

static AstCommand *parse_command(Parser *p) {
    AstCommand *cmd = ast_create_command();

    while (!p->has_error) {
        const Token *tok = peek(p);
        if (!tok || tok->type == TOKEN_EOF) break;

        if (tok->type == TOKEN_WORD) {
            ast_command_add_arg(cmd, tok->value);
            advance(p);
        } else if (tok->type == TOKEN_REDIRECT_IN ||
                   tok->type == TOKEN_REDIRECT_OUT ||
                   tok->type == TOKEN_REDIRECT_APPEND ||
                   tok->type == TOKEN_REDIRECT_STDERR ||
                   tok->type == TOKEN_REDIRECT_STDERR_APPEND) {
            TokenType rtype = tok->type;
            advance(p);
            const Token *file_tok = peek(p);
            if (!file_tok || file_tok->type != TOKEN_WORD) {
                set_error(p, "syntax error near unexpected token '%s'",
                          file_tok && file_tok->value ? file_tok->value : "newline");
                ast_free_command(cmd);
                return NULL;
            }
            RedirType rt = REDIR_INPUT;
            if (rtype == TOKEN_REDIRECT_OUT) rt = REDIR_OUTPUT;
            else if (rtype == TOKEN_REDIRECT_APPEND) rt = REDIR_APPEND;
            else if (rtype == TOKEN_REDIRECT_STDERR) rt = REDIR_STDERR;
            else if (rtype == TOKEN_REDIRECT_STDERR_APPEND) rt = REDIR_STDERR_APPEND;

            Redirection *redir = ast_create_redirection(rt, file_tok->value);
            ast_command_add_redir(cmd, redir);
            advance(p);
        } else if (tok->type == TOKEN_REDIRECT_STDERR_TO_STDOUT) {
            Redirection *redir = ast_create_redirection(REDIR_STDERR_TO_STDOUT, NULL);
            ast_command_add_redir(cmd, redir);
            advance(p);
        } else {
            /* Delimiter or operator reached */
            break;
        }
    }

    if (cmd->argc == 0 && cmd->redirections == NULL) {
        /* Empty command */
        ast_free_command(cmd);
        return NULL;
    }

    return cmd;
}

static AstPipeline *parse_pipeline(Parser *p) {
    AstPipeline *pipeline = ast_create_pipeline();

    AstCommand *cmd = parse_command(p);
    if (!cmd) {
        ast_free_pipeline(pipeline);
        return NULL;
    }
    ast_pipeline_add_command(pipeline, cmd);

    while (match(p, TOKEN_PIPE)) {
        AstCommand *next_cmd = parse_command(p);
        if (!next_cmd) {
            set_error(p, "syntax error near unexpected token '|'");
            ast_free_pipeline(pipeline);
            return NULL;
        }
        ast_pipeline_add_command(pipeline, next_cmd);
    }

    /* Check for trailing background operator '&' */
    if (match(p, TOKEN_BACKGROUND)) {
        pipeline->background = true;
    }

    return pipeline;
}

static AstConditional *parse_conditional(Parser *p) {
    AstPipeline *first_pipe = parse_pipeline(p);
    if (!first_pipe) {
        return NULL;
    }

    CondType op = COND_NONE;
    if (match(p, TOKEN_AND)) {
        op = COND_AND;
    } else if (match(p, TOKEN_OR)) {
        op = COND_OR;
    }

    AstConditional *cond = ast_create_conditional(first_pipe, op);

    if (op != COND_NONE) {
        AstConditional *next_cond = parse_conditional(p);
        if (!next_cond) {
            set_error(p, "syntax error near unexpected token '%s'",
                      op == COND_AND ? "&&" : "||");
            ast_free_conditional(cond);
            return NULL;
        }
        cond->next = next_cond;
    }

    return cond;
}

AstProgram *parse_tokens(const TokenList *tokens, char *err_msg, size_t err_size) {
    if (!tokens || tokens->count == 0) return NULL;

    Parser parser;
    parser.tokens = tokens;
    parser.pos = 0;
    parser.error_msg = err_msg;
    parser.error_size = err_size;
    parser.has_error = false;

    if (err_msg && err_size > 0) err_msg[0] = '\0';

    AstProgram *prog = ast_create_program();

    while (parser.pos < tokens->count && !parser.has_error) {
        const Token *tok = peek(&parser);
        if (!tok || tok->type == TOKEN_EOF) break;

        /* Skip leading semicolons */
        if (tok->type == TOKEN_SEMICOLON) {
            advance(&parser);
            continue;
        }

        AstConditional *cond = parse_conditional(&parser);
        if (parser.has_error) {
            if (cond) ast_free_conditional(cond);
            ast_free_program(prog);
            return NULL;
        }

        if (cond) {
            ast_program_add_conditional(prog, cond);
        }

        /* Consume separator if present */
        if (match(&parser, TOKEN_SEMICOLON)) {
            /* Handled */
        } else {
            /* If not EOF or separator, check what token is remaining */
            const Token *rem = peek(&parser);
            if (rem && rem->type != TOKEN_EOF) {
                /* If pipeline was marked background and ended, allow continuing */
                if (cond && cond->pipeline && cond->pipeline->background) {
                    continue;
                }
                set_error(&parser, "syntax error near unexpected token '%s'",
                          rem->value ? rem->value : "unknown");
                ast_free_program(prog);
                return NULL;
            }
        }
    }

    if (parser.has_error) {
        ast_free_program(prog);
        return NULL;
    }

    return prog;
}
