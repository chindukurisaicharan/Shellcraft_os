#ifndef SHELLCRAFT_AST_H
#define SHELLCRAFT_AST_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    REDIR_INPUT,               /* < file */
    REDIR_OUTPUT,              /* > file */
    REDIR_APPEND,              /* >> file */
    REDIR_STDERR,              /* 2> file */
    REDIR_STDERR_APPEND,       /* 2>> file */
    REDIR_STDERR_TO_STDOUT     /* 2>&1 */
} RedirType;

typedef struct Redirection {
    RedirType type;
    char *target;              /* filename or NULL for 2>&1 */
    struct Redirection *next;
} Redirection;

typedef struct AstCommand {
    char **argv;
    int argc;
    int capacity;
    Redirection *redirections;
} AstCommand;

typedef struct AstPipeline {
    AstCommand **commands;
    int count;
    int capacity;
    bool background;           /* trailing '&' flag */
} AstPipeline;

typedef enum {
    COND_NONE,
    COND_AND,                  /* && */
    COND_OR,                   /* || */
    COND_SEQ                   /* ; or newline */
} CondType;

typedef struct AstConditional {
    AstPipeline *pipeline;
    CondType op_to_next;
    struct AstConditional *next;
} AstConditional;

typedef struct AstProgram {
    AstConditional *conditionals;
} AstProgram;

/* Constructors */
Redirection *ast_create_redirection(RedirType type, const char *target);
AstCommand *ast_create_command(void);
void ast_command_add_arg(AstCommand *cmd, const char *arg);
void ast_command_add_redir(AstCommand *cmd, Redirection *redir);

AstPipeline *ast_create_pipeline(void);
void ast_pipeline_add_command(AstPipeline *pipeline, AstCommand *cmd);

AstConditional *ast_create_conditional(AstPipeline *pipeline, CondType op);
AstProgram *ast_create_program(void);
void ast_program_add_conditional(AstProgram *prog, AstConditional *cond);

/* Destructors */
void ast_free_redirection(Redirection *redir);
void ast_free_command(AstCommand *cmd);
void ast_free_pipeline(AstPipeline *pipeline);
void ast_free_conditional(AstConditional *cond);
void ast_free_program(AstProgram *prog);

/* Debug AST visualizer */
void ast_debug_print(const AstProgram *prog);

#endif /* SHELLCRAFT_AST_H */
