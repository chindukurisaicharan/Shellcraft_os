#ifndef SHELLCRAFT_EXPANSION_H
#define SHELLCRAFT_EXPANSION_H

#include <stdbool.h>
#include <sys/types.h>

/* Forward declare ShellState */
struct ShellState;

/* Expands tilde and environment variables within a token string */
char *expand_variables_and_tilde(const char *input, int last_exit_status, pid_t shell_pid);

/* Performs command substitution $(cmd) in an input string */
char *expand_command_substitutions(const char *input, struct ShellState *shell);

/* Expands glob wildcards for an argument vector. Returns new argv dynamically. */
char **expand_glob_tokens(char **raw_argv, int raw_argc, int *new_argc);

/* Full argument expansion pipeline for a command node */
char **expand_command_args(char **raw_argv, int raw_argc, int *new_argc, struct ShellState *shell);

#endif /* SHELLCRAFT_EXPANSION_H */
