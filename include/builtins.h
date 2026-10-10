#ifndef SHELLCRAFT_BUILTINS_H
#define SHELLCRAFT_BUILTINS_H

#include <stdbool.h>

struct ShellState;

/* Check if a command is a built-in */
bool is_builtin(const char *name);

/* Execute a built-in command. Returns exit status (0-255). */
int execute_builtin(int argc, char **argv, struct ShellState *shell);

/* Alias management functions */
void alias_init(void);
void alias_cleanup(void);
int builtin_alias(int argc, char **argv);
int builtin_unalias(int argc, char **argv);
const char *alias_lookup(const char *name);

/* Builtin function signatures */
int builtin_cd(int argc, char **argv, struct ShellState *shell);
int builtin_exit(int argc, char **argv, struct ShellState *shell);
int builtin_export(int argc, char **argv);
int builtin_pwd(int argc, char **argv);
int builtin_echo(int argc, char **argv);
int builtin_help(int argc, char **argv);
int builtin_history(int argc, char **argv);
int builtin_clear(int argc, char **argv);
int builtin_type(int argc, char **argv);
int builtin_jobs(int argc, char **argv);
int builtin_fg(int argc, char **argv);
int builtin_bg(int argc, char **argv);
int builtin_stats(int argc, char **argv, struct ShellState *shell);
int builtin_set(int argc, char **argv, struct ShellState *shell);

#endif /* SHELLCRAFT_BUILTINS_H */
