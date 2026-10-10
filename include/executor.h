#ifndef SHELLCRAFT_EXECUTOR_H
#define SHELLCRAFT_EXECUTOR_H

#include "ast.h"

struct ShellState;

/* Executes an entire AST program */
int execute_program(AstProgram *prog, struct ShellState *shell);

/* Executes a conditional node (handles &&, ||, ;) */
int execute_conditional(AstConditional *cond, struct ShellState *shell);

/* Executes a pipeline of one or more commands */
int execute_pipeline(AstPipeline *pipeline, struct ShellState *shell);

/* Executes a single simple command node (for standalone / builtins) */
int execute_command_simple(AstCommand *cmd, struct ShellState *shell, bool background);

#endif /* SHELLCRAFT_EXECUTOR_H */
