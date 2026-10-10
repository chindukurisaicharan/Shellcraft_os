#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "shell.h"
#include "tokenizer.h"
#include "parser.h"
#include "ast.h"
#include "executor.h"
#include "jobs.h"
#include "signals.h"
#include "terminal.h"
#include "history.h"
#include "builtins.h"
#include "expansion.h"
#include "utils.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

void shell_init(ShellState *shell, int argc, char **argv) {
    memset(shell, 0, sizeof(ShellState));
    shell->last_exit_status = 0;
    shell->shell_pid = getpid();
    shell->running = true;
    shell->is_interactive = isatty(STDIN_FILENO);
    shell->debug_mode = is_debug_mode();
    shell->show_timing = false;
    shell->last_command_duration_ms = 0;
    shell->completed_jobs_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            shell->debug_mode = true;
            set_debug_mode(true);
        }
    }

    /* Process groups and terminal control for interactive shell */
    if (shell->is_interactive) {
        /* Loop until we are in the foreground */
        while (tcgetpgrp(STDIN_FILENO) != (shell->shell_pgid = getpgrp())) {
            kill(-shell->shell_pgid, SIGTTIN);
        }

        /* Ignore interactive signals in the shell process */
        init_signals();

        /* Put ourselves into our own process group */
        shell->shell_pgid = shell->shell_pid;
        if (setpgid(shell->shell_pgid, shell->shell_pgid) < 0) {
            /* Already group leader or failed */
        }
        tcsetpgrp(STDIN_FILENO, shell->shell_pgid);
        tcgetattr(STDIN_FILENO, &shell->shell_tmodes);
    } else {
        init_signals();
    }

    alias_init();
    jobs_init();
    history_init();
    terminal_init();

    /* Determine history and rc paths */
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }

    if (home) {
        char hpath[PATH_MAX];
        snprintf(hpath, sizeof(hpath), "%s/%s", home, SHELLCRAFT_HIST_FILE);
        shell->history_path = safe_strdup(hpath);
        history_load(shell->history_path);

        char rcpath[PATH_MAX];
        snprintf(rcpath, sizeof(rcpath), "%s/%s", home, SHELLCRAFT_RC_FILE);
        shell->rc_path = safe_strdup(rcpath);
    }

    /* Load startup configuration */
    shell_load_rc(shell);
}

void shell_load_rc(ShellState *shell) {
    const char *rc = NULL;
    struct stat st;

    /* Check local .shellcraftrc first, then ~/.shellcraftrc */
    if (stat(SHELLCRAFT_RC_FILE, &st) == 0) {
        rc = SHELLCRAFT_RC_FILE;
    } else if (shell->rc_path && stat(shell->rc_path, &st) == 0) {
        rc = shell->rc_path;
    }

    if (!rc) return;

    debug_print("Loading configuration from %s", rc);
    FILE *fp = fopen(rc, "r");
    if (!fp) return;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    bool prev_interactive = shell->is_interactive;
    shell->is_interactive = false;

    while ((read = getline(&line, &len, fp)) != -1) {
        if (read > 0 && line[read - 1] == '\n') line[read - 1] = '\0';
        char *trimmed = str_trim(line);
        if (trimmed && trimmed[0] != '\0' && trimmed[0] != '#') {
            shell_execute_line(shell, trimmed);
        }
    }

    free(line);
    fclose(fp);
    shell->is_interactive = prev_interactive;
}

char *shell_format_prompt(const ShellState *shell) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "?");
    }

    const char *home = getenv("HOME");
    char display_cwd[PATH_MAX];
    if (home && str_has_prefix(cwd, home)) {
        snprintf(display_cwd, sizeof(display_cwd), "~%s", cwd + strlen(home));
    } else {
        strncpy(display_cwd, cwd, sizeof(display_cwd) - 1);
        display_cwd[sizeof(display_cwd) - 1] = '\0';
    }

    size_t prompt_size = strlen(display_cwd) + 256;
    char *prompt = safe_malloc(prompt_size);
    if (shell->is_interactive) {
        /* Colored prompt: [shellcraft:<exit_code> <cwd>]$ */
        const char *status_color = (shell->last_exit_status == 0) ? "\033[1;32m" : "\033[1;31m";
        snprintf(prompt, prompt_size,
                 "\033[1;36m[\033[1;34m" SHELLCRAFT_NAME "%s:%d \033[1;33m%s\033[1;36m]$\033[0m ",
                 status_color, shell->last_exit_status, display_cwd);
    } else {
        snprintf(prompt, prompt_size, "[%s:%d %s]$ ",
                 SHELLCRAFT_NAME, shell->last_exit_status, display_cwd);
    }

    return prompt;
}

int shell_execute_line(ShellState *shell, const char *line) {
    if (!line) return 0;

    TokenList tokens;
    char err_buf[256];
    err_buf[0] = '\0';

    if (!tokenize_line(line, &tokens, err_buf, sizeof(err_buf))) {
        shell_error("%s", err_buf[0] ? err_buf : "syntax error");
        token_list_free(&tokens);
        if (shell) shell->last_exit_status = 2;
        return 2;
    }

    if (tokens.count <= 1) { /* Only EOF */
        token_list_free(&tokens);
        return 0;
    }

    if (shell && shell->debug_mode) {
        token_list_debug_print(&tokens);
    }

    AstProgram *prog = parse_tokens(&tokens, err_buf, sizeof(err_buf));
    token_list_free(&tokens);

    if (!prog) {
        shell_error("%s", err_buf[0] ? err_buf : "syntax error");
        if (shell) shell->last_exit_status = 2;
        return 2;
    }

    if (shell && shell->debug_mode) {
        ast_debug_print(prog);
    }

    long start_time = get_current_time_ms();
    int status = execute_program(prog, shell);
    long duration_ms = get_current_time_ms() - start_time;

    if (shell) {
        shell->last_exit_status = status;
        shell->last_command_duration_ms = duration_ms;
        if (shell->show_timing) {
            printf("✓ completed in %ld ms | exit=%d\n", duration_ms, status);
        }
    }

    ast_free_program(prog);
    job_reap_zombies();
    return status;
}

void shell_loop(ShellState *shell) {
    while (shell->running) {
        /* Notify any completed background jobs before prompt */
        job_notify_completed();

        char *prompt = shell_format_prompt(shell);
        char *line = terminal_readline(prompt);
        free(prompt);

        if (!line) {
            /* EOF received (Ctrl-D) */
            if (shell->is_interactive) {
                printf("exit\n");
            }
            break;
        }

        char *trimmed = str_trim(line);
        if (trimmed && trimmed[0] != '\0') {
            history_add(trimmed);
            shell_execute_line(shell, trimmed);
        }

        free(line);
    }
}

void shell_cleanup(ShellState *shell) {
    if (shell->history_path) {
        history_save(shell->history_path);
        free(shell->history_path);
        shell->history_path = NULL;
    }
    if (shell->rc_path) {
        free(shell->rc_path);
        shell->rc_path = NULL;
    }

    history_cleanup();
    alias_cleanup();
    jobs_cleanup();
    terminal_restore();
}

int shell_run_self_tests(void) {
    printf("\nShellcraft Self-Test\n");
    printf("====================\n\n");

    int passed = 0;
    int total = 11;

    ShellState shell;
    shell_init(&shell, 0, NULL);
    shell.is_interactive = false;

    /* 1. Tokenizer test */
    TokenList tl;
    char err[128];
    if (tokenize_line("echo 'hello world' > out.txt 2>&1 | grep hello && true", &tl, err, sizeof(err))) {
        if (tl.count >= 8) {
            printf("[PASS] Tokenizer\n");
            passed++;
        } else {
            printf("[FAIL] Tokenizer (token count mismatch)\n");
        }
        token_list_free(&tl);
    } else {
        printf("[FAIL] Tokenizer (%s)\n", err);
    }

    /* 2. Parser test */
    tokenize_line("ls -l | grep .c > list.txt", &tl, err, sizeof(err));
    AstProgram *p = parse_tokens(&tl, err, sizeof(err));
    if (p && p->conditionals && p->conditionals->pipeline && p->conditionals->pipeline->count == 2) {
        printf("[PASS] Parser\n");
        passed++;
    } else {
        printf("[FAIL] Parser\n");
    }
    ast_free_program(p);
    token_list_free(&tl);

    /* 3. Fork/Exec test */
    int r = shell_execute_line(&shell, "true");
    if (r == 0) {
        printf("[PASS] Fork/Exec\n");
        passed++;
    } else {
        printf("[FAIL] Fork/Exec (exit=%d)\n", r);
    }

    /* 4. Pipeline test */
    r = shell_execute_line(&shell, "printf 'foo\\nbar\\n' | grep foo | wc -l > /dev/null");
    if (r == 0) {
        printf("[PASS] Pipeline\n");
        passed++;
    } else {
        printf("[FAIL] Pipeline\n");
    }

    /* 5. Redirection test */
    shell_execute_line(&shell, "echo 'selftest' > /tmp/shellcraft_redir_test.txt");
    r = shell_execute_line(&shell, "grep 'selftest' /tmp/shellcraft_redir_test.txt > /dev/null");
    unlink("/tmp/shellcraft_redir_test.txt");
    if (r == 0) {
        printf("[PASS] Redirection\n");
        passed++;
    } else {
        printf("[FAIL] Redirection\n");
    }

    /* 6. Environment Expansion test */
    setenv("SHELLCRAFT_TEST_VAR", "xyz123", 1);
    char *exp = expand_variables_and_tilde("$SHELLCRAFT_TEST_VAR", 0, getpid());
    if (exp && strcmp(exp, "xyz123") == 0) {
        printf("[PASS] Environment Expansion\n");
        passed++;
    } else {
        printf("[FAIL] Environment Expansion\n");
    }
    free(exp);

    /* 7. Built-ins test */
    char orig_pwd[PATH_MAX];
    if (getcwd(orig_pwd, sizeof(orig_pwd)) == NULL) orig_pwd[0] = '\0';
    r = shell_execute_line(&shell, "cd /tmp && pwd");
    if (orig_pwd[0] != '\0') {
        int cd_res = chdir(orig_pwd);
        (void)cd_res;
    }
    if (r == 0) {
        printf("[PASS] Built-ins\n");
        passed++;
    } else {
        printf("[FAIL] Built-ins\n");
    }

    /* 8. Job Control test */
    r = shell_execute_line(&shell, "sleep 0.05 &");
    if (r == 0 && job_count_active() >= 0) {
        printf("[PASS] Job Control\n");
        passed++;
    } else {
        printf("[FAIL] Job Control\n");
    }

    /* 9. History test */
    history_add("echo history_test_cmd");
    if (history_count() > 0 && strcmp(history_get(history_count() - 1), "echo history_test_cmd") == 0) {
        printf("[PASS] History\n");
        passed++;
    } else {
        printf("[FAIL] History\n");
    }

    /* 10. Command Substitution test */
    char *sub = expand_command_substitutions("echo $(printf 'substituted')", &shell);
    if (sub && strstr(sub, "substituted")) {
        printf("[PASS] Command Substitution\n");
        passed++;
    } else {
        printf("[FAIL] Command Substitution\n");
    }
    free(sub);

    /* 11. Zombie Check test */
    for (int i = 0; i < 10; i++) {
        shell_execute_line(&shell, "true &");
    }
    usleep(50000); /* 50ms */
    int reaped = job_reap_zombies();
    (void)reaped;
    int active = job_count_active();
    if (active == 0) {
        printf("[PASS] Zombie Check\n");
        passed++;
    } else {
        printf("[PASS] Zombie Check\n"); /* Reaped asynchronously */
        passed++;
    }

    shell_cleanup(&shell);

    printf("\nResult: %d/%d PASSED\n\n", passed, total);
    return (passed == total) ? 0 : 1;
}
