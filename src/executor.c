#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "executor.h"
#include "builtins.h"
#include "redirection.h"
#include "jobs.h"
#include "signals.h"
#include "expansion.h"
#include "terminal.h"
#include "shell.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

static char *build_pipeline_cmd_string(const AstPipeline *pipeline) {
    size_t cap = 256;
    char *str = safe_malloc(cap);
    str[0] = '\0';

    for (int i = 0; i < pipeline->count; i++) {
        AstCommand *cmd = pipeline->commands[i];
        for (int a = 0; a < cmd->argc; a++) {
            size_t req = strlen(str) + strlen(cmd->argv[a]) + 2;
            if (req >= cap) {
                cap = req + 128;
                str = safe_realloc(str, cap);
            }
            strcat(str, cmd->argv[a]);
            strcat(str, " ");
        }
        if (i < pipeline->count - 1) {
            strcat(str, "| ");
        }
    }
    if (pipeline->background) {
        strcat(str, "&");
    }
    return str;
}

int execute_command_simple(AstCommand *cmd, ShellState *shell, bool background) {
    if (!cmd || cmd->argc == 0) return 0;

    /* Check for alias replacement on first argument */
    const char *alias_val = alias_lookup(cmd->argv[0]);
    int new_argc = 0;
    char **expanded_argv = NULL;

    if (alias_val) {
        int alias_argc = 0;
        char **alias_words = str_split_whitespace(alias_val, &alias_argc);
        int total_raw = alias_argc + (cmd->argc - 1);
        char **combined = safe_malloc(sizeof(char *) * (total_raw + 1));
        for (int i = 0; i < alias_argc; i++) combined[i] = alias_words[i];
        for (int i = 1; i < cmd->argc; i++) combined[alias_argc + i - 1] = cmd->argv[i];
        combined[total_raw] = NULL;
        free(alias_words);

        expanded_argv = expand_command_args(combined, total_raw, &new_argc, shell);
        for (int i = 0; i < alias_argc; i++) free(combined[i]);
        free(combined);
    } else {
        expanded_argv = expand_command_args(cmd->argv, cmd->argc, &new_argc, shell);
    }

    if (new_argc == 0 || !expanded_argv || !expanded_argv[0]) {
        if (expanded_argv) free_str_array(expanded_argv, new_argc);
        return 0;
    }

    /* Check if it is a built-in executed directly in the shell */
    if (!background && is_builtin(expanded_argv[0])) {
        int status = 0;
        if (cmd->redirections) {
            int saved_fds[3];
            if (save_standard_fds(saved_fds) < 0) {
                free_str_array(expanded_argv, new_argc);
                return 1;
            }
            fflush(stdout);
            fflush(stderr);
            if (apply_redirections(cmd->redirections) < 0) {
                restore_standard_fds(saved_fds);
                free_str_array(expanded_argv, new_argc);
                return 1;
            }
            status = execute_builtin(new_argc, expanded_argv, shell);
            fflush(stdout);
            fflush(stderr);
            restore_standard_fds(saved_fds);
        } else {
            status = execute_builtin(new_argc, expanded_argv, shell);
        }
        free_str_array(expanded_argv, new_argc);
        return status;
    }

    /* External command or background built-in: run via fork/exec */
    sigset_t oldmask;
    block_sigchld(&oldmask);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        unblock_sigchld(&oldmask);
        free_str_array(expanded_argv, new_argc);
        return 1;
    }

    if (pid == 0) {
        /* Child Process */
        if (shell && shell->is_interactive) {
            pid_t cpid = getpid();
            setpgid(cpid, cpid);
            if (!background && terminal_is_interactive() && tcgetpgrp(STDIN_FILENO) == getpgrp()) {
                tcsetpgrp(STDIN_FILENO, cpid);
            }
        }

        reset_signals_for_child();
        unblock_sigchld(&oldmask);

        if (apply_redirections(cmd->redirections) < 0) {
            _exit(1);
        }

        if (is_builtin(expanded_argv[0])) {
            int ret = execute_builtin(new_argc, expanded_argv, shell);
            _exit(ret);
        }

        debug_print("execvp('%s')", expanded_argv[0]);
        execvp(expanded_argv[0], expanded_argv);

        if (errno == ENOENT) {
            shell_error("%s: command not found", expanded_argv[0]);
            _exit(127);
        } else if (errno == EACCES) {
            shell_error("%s: Permission denied", expanded_argv[0]);
            _exit(126);
        } else {
            shell_error("%s: %s", expanded_argv[0], strerror(errno));
            _exit(1);
        }
    }

    /* Parent Process */
    if (shell && shell->is_interactive) {
        setpgid(pid, pid);
    }

    char *cmd_line = build_pipeline_cmd_string(&(AstPipeline){
        .commands = &cmd,
        .count = 1,
        .capacity = 1,
        .background = background
    });

    Job *job = job_create(pid, cmd_line, 1, &pid, background ? JOB_RUNNING : JOB_RUNNING);
    free(cmd_line);
    unblock_sigchld(&oldmask);
    free_str_array(expanded_argv, new_argc);

    if (background) {
        job_put_in_background(job, false);
        return 0;
    } else {
        int status = 0;
        job_wait_foreground(job, &status);
        return status;
    }
}

int execute_pipeline(AstPipeline *pipeline, ShellState *shell) {
    if (!pipeline || pipeline->count == 0) return 0;

    /* Optimize single command */
    if (pipeline->count == 1) {
        return execute_command_simple(pipeline->commands[0], shell, pipeline->background);
    }

    int N = pipeline->count;
    int pipes[N - 1][2];

    for (int i = 0; i < N - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return 1;
        }
        debug_print("Created pipe[%d]: fd[%d, %d]", i, pipes[i][0], pipes[i][1]);
    }

    pid_t *pids = safe_malloc(sizeof(pid_t) * N);
    pid_t pgid = 0;

    sigset_t oldmask;
    block_sigchld(&oldmask);

    for (int i = 0; i < N; i++) {
        AstCommand *cmd = pipeline->commands[i];

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork in pipeline");
            break;
        }

        if (pid == 0) {
            /* Child process i */
            if (shell && shell->is_interactive) {
                if (i == 0) {
                    pgid = getpid();
                }
                setpgid(0, pgid);

                if (!pipeline->background && terminal_is_interactive() && tcgetpgrp(STDIN_FILENO) == getpgrp()) {
                    tcsetpgrp(STDIN_FILENO, pgid);
                }
            }

            reset_signals_for_child();
            unblock_sigchld(&oldmask);

            /* Connect standard input from previous pipe */
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2 pipe in");
                    _exit(1);
                }
            }

            /* Connect standard output to next pipe */
            if (i < N - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2 pipe out");
                    _exit(1);
                }
            }

            /* Close all pipe file descriptors in child */
            for (int j = 0; j < N - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            /* Apply file redirections for this command */
            if (apply_redirections(cmd->redirections) < 0) {
                _exit(1);
            }

            /* Expand command arguments */
            int exp_argc = 0;
            char **exp_argv = expand_command_args(cmd->argv, cmd->argc, &exp_argc, shell);
            if (exp_argc == 0 || !exp_argv || !exp_argv[0]) {
                _exit(0);
            }

            if (is_builtin(exp_argv[0])) {
                int ret = execute_builtin(exp_argc, exp_argv, shell);
                _exit(ret);
            }

            debug_print("Pipeline stage %d execvp: %s", i, exp_argv[0]);
            execvp(exp_argv[0], exp_argv);

            if (errno == ENOENT) {
                shell_error("%s: command not found", exp_argv[0]);
                _exit(127);
            } else if (errno == EACCES) {
                shell_error("%s: Permission denied", exp_argv[0]);
                _exit(126);
            } else {
                shell_error("%s: %s", exp_argv[0], strerror(errno));
                _exit(1);
            }
        }

        /* Parent process */
        pids[i] = pid;
        if (shell && shell->is_interactive) {
            if (i == 0) {
                pgid = pid;
            }
            setpgid(pid, pgid);
        }
    }

    /* Parent closes all pipe ends */
    for (int j = 0; j < N - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    char *cmd_line = build_pipeline_cmd_string(pipeline);
    Job *job = job_create(pgid, cmd_line, N, pids, pipeline->background ? JOB_RUNNING : JOB_RUNNING);
    free(cmd_line);
    free(pids);

    unblock_sigchld(&oldmask);

    if (pipeline->background) {
        job_put_in_background(job, false);
        return 0;
    } else {
        int status = 0;
        job_wait_foreground(job, &status);
        return status;
    }
}

int execute_conditional(AstConditional *cond, ShellState *shell) {
    if (!cond) return 0;
    return execute_pipeline(cond->pipeline, shell);
}

int execute_program(AstProgram *prog, ShellState *shell) {
    if (!prog) return 0;
    int status = 0;

    AstConditional *curr = prog->conditionals;
    while (curr && (!shell || shell->running)) {
        status = execute_pipeline(curr->pipeline, shell);
        if (shell) shell->last_exit_status = status;

        if (curr->op_to_next == COND_AND) {
            if (status != 0) {
                /* Skip subsequent chained AND commands */
                while (curr && curr->op_to_next == COND_AND) {
                    curr = curr->next;
                }
            }
        } else if (curr->op_to_next == COND_OR) {
            if (status == 0) {
                /* Skip subsequent chained OR commands */
                while (curr && curr->op_to_next == COND_OR) {
                    curr = curr->next;
                }
            }
        }

        if (curr) {
            curr = curr->next;
        }
    }

    return status;
}
