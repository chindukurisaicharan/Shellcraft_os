#ifndef SHELLCRAFT_SHELL_H
#define SHELLCRAFT_SHELL_H

#include <stdbool.h>
#include <sys/types.h>
#include <termios.h>

typedef struct ShellState {
    int last_exit_status;
    pid_t shell_pid;
    pid_t shell_pgid;
    int shell_terminal;
    struct termios shell_tmodes;
    bool is_interactive;
    bool debug_mode;
    bool show_timing;
    long last_command_duration_ms;
    int completed_jobs_count;
    bool running;
    char *rc_path;
    char *history_path;
} ShellState;

/* Shell lifecycle */
void shell_init(ShellState *shell, int argc, char **argv);
void shell_loop(ShellState *shell);
void shell_cleanup(ShellState *shell);

/* Execution of a single line of input */
int shell_execute_line(ShellState *shell, const char *line);

/* Loads .shellcraftrc file */
void shell_load_rc(ShellState *shell);

/* Formats prompt string dynamically: [shellcraft:status cwd]$ */
char *shell_format_prompt(const ShellState *shell);

/* Evaluator self-test suite */
int shell_run_self_tests(void);

#endif /* SHELLCRAFT_SHELL_H */
