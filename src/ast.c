#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "ast.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Redirection *ast_create_redirection(RedirType type, const char *target) {
    Redirection *redir = safe_malloc(sizeof(Redirection));
    redir->type = type;
    redir->target = target ? safe_strdup(target) : NULL;
    redir->next = NULL;
    return redir;
}

AstCommand *ast_create_command(void) {
    AstCommand *cmd = safe_malloc(sizeof(AstCommand));
    cmd->argc = 0;
    cmd->capacity = 4;
    cmd->argv = safe_malloc(sizeof(char *) * cmd->capacity);
    cmd->argv[0] = NULL;
    cmd->redirections = NULL;
    return cmd;
}

void ast_command_add_arg(AstCommand *cmd, const char *arg) {
    if (!cmd || !arg) return;
    if (cmd->argc + 1 >= cmd->capacity) {
        cmd->capacity *= 2;
        cmd->argv = safe_realloc(cmd->argv, sizeof(char *) * cmd->capacity);
    }
    cmd->argv[cmd->argc++] = safe_strdup(arg);
    cmd->argv[cmd->argc] = NULL;
}

void ast_command_add_redir(AstCommand *cmd, Redirection *redir) {
    if (!cmd || !redir) return;
    if (!cmd->redirections) {
        cmd->redirections = redir;
    } else {
        Redirection *curr = cmd->redirections;
        while (curr->next) curr = curr->next;
        curr->next = redir;
    }
}

AstPipeline *ast_create_pipeline(void) {
    AstPipeline *pipe = safe_malloc(sizeof(AstPipeline));
    pipe->count = 0;
    pipe->capacity = 4;
    pipe->commands = safe_malloc(sizeof(AstCommand *) * pipe->capacity);
    pipe->background = false;
    return pipe;
}

void ast_pipeline_add_command(AstPipeline *pipeline, AstCommand *cmd) {
    if (!pipeline || !cmd) return;
    if (pipeline->count >= pipeline->capacity) {
        pipeline->capacity *= 2;
        pipeline->commands = safe_realloc(pipeline->commands, sizeof(AstCommand *) * pipeline->capacity);
    }
    pipeline->commands[pipeline->count++] = cmd;
}

AstConditional *ast_create_conditional(AstPipeline *pipeline, CondType op) {
    AstConditional *cond = safe_malloc(sizeof(AstConditional));
    cond->pipeline = pipeline;
    cond->op_to_next = op;
    cond->next = NULL;
    return cond;
}

AstProgram *ast_create_program(void) {
    AstProgram *prog = safe_malloc(sizeof(AstProgram));
    prog->conditionals = NULL;
    return prog;
}

void ast_program_add_conditional(AstProgram *prog, AstConditional *cond) {
    if (!prog || !cond) return;
    if (!prog->conditionals) {
        prog->conditionals = cond;
    } else {
        AstConditional *curr = prog->conditionals;
        while (curr->next) curr = curr->next;
        curr->next = cond;
    }
}

void ast_free_redirection(Redirection *redir) {
    while (redir) {
        Redirection *next = redir->next;
        if (redir->target) free(redir->target);
        free(redir);
        redir = next;
    }
}

void ast_free_command(AstCommand *cmd) {
    if (!cmd) return;
    for (int i = 0; i < cmd->argc; i++) {
        if (cmd->argv[i]) free(cmd->argv[i]);
    }
    free(cmd->argv);
    ast_free_redirection(cmd->redirections);
    free(cmd);
}

void ast_free_pipeline(AstPipeline *pipeline) {
    if (!pipeline) return;
    for (int i = 0; i < pipeline->count; i++) {
        ast_free_command(pipeline->commands[i]);
    }
    free(pipeline->commands);
    free(pipeline);
}

void ast_free_conditional(AstConditional *cond) {
    while (cond) {
        AstConditional *next = cond->next;
        ast_free_pipeline(cond->pipeline);
        free(cond);
        cond = next;
    }
}

void ast_free_program(AstProgram *prog) {
    if (!prog) return;
    ast_free_conditional(prog->conditionals);
    free(prog);
}

static const char *redir_type_name(RedirType type) {
    switch (type) {
        case REDIR_INPUT: return "<";
        case REDIR_OUTPUT: return ">";
        case REDIR_APPEND: return ">>";
        case REDIR_STDERR: return "2>";
        case REDIR_STDERR_APPEND: return "2>>";
        case REDIR_STDERR_TO_STDOUT: return "2>&1";
        default: return "?";
    }
}

static const char *cond_type_name(CondType type) {
    switch (type) {
        case COND_AND: return "&&";
        case COND_OR: return "||";
        case COND_SEQ: return ";";
        default: return "NONE";
    }
}

void ast_debug_print(const AstProgram *prog) {
    if (!prog) {
        debug_print("AST is NULL");
        return;
    }
    debug_print("=== AST DUMP ===");
    AstConditional *c = prog->conditionals;
    int c_idx = 0;
    while (c) {
        debug_print("Conditional [%d] (op_to_next: %s, bg: %d):",
                    c_idx++, cond_type_name(c->op_to_next), c->pipeline ? c->pipeline->background : 0);
        if (c->pipeline) {
            for (int p = 0; p < c->pipeline->count; p++) {
                AstCommand *cmd = c->pipeline->commands[p];
                char cmd_str[512] = "";
                for (int a = 0; a < cmd->argc; a++) {
                    strncat(cmd_str, cmd->argv[a], sizeof(cmd_str) - strlen(cmd_str) - 2);
                    strcat(cmd_str, " ");
                }
                debug_print("    Command [%d]: %s", p, cmd_str);
                Redirection *r = cmd->redirections;
                while (r) {
                    debug_print("        Redir: %s %s", redir_type_name(r->type), r->target ? r->target : "");
                    r = r->next;
                }
            }
        }
        c = c->next;
    }
    debug_print("================");
}
