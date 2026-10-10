#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "shell.h"
#include "utils.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_version(void) {
    printf("%s %s — %s\n", SHELLCRAFT_NAME, SHELLCRAFT_VERSION, SHELLCRAFT_SUBTITLE);
    printf("Language: C11 | Platform: POSIX/Linux\n");
}

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS] [-c <command>]\n\n", prog);
    printf("Options:\n");
    printf("  -c <command>    Execute specified command line and exit\n");
    printf("  --debug, -d     Enable debug diagnostics (tracing fork, pipe, dup2, exec)\n");
    printf("  --self-test     Run automated self-tests across all shell subsystems\n");
    printf("  --version, -v   Display version information\n");
    printf("  --help, -h      Display this help message\n");
}

int main(int argc, char **argv) {
    bool run_self_test = false;
    const char *cmd_to_run = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            set_debug_mode(true);
        }
        if (strcmp(argv[i], "--self-test") == 0) {
            run_self_test = true;
        }
        if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 < argc) {
                cmd_to_run = argv[++i];
            } else {
                fprintf(stderr, "shellcraft: -c requires an argument\n");
                return 2;
            }
        }
    }

    if (run_self_test) {
        return shell_run_self_tests();
    }

    ShellState shell;
    shell_init(&shell, argc, argv);

    if (cmd_to_run) {
        int status = shell_execute_line(&shell, cmd_to_run);
        shell_cleanup(&shell);
        return status;
    }

    /* Interactive or batch REPL loop */
    shell_loop(&shell);

    int exit_status = shell.last_exit_status;
    shell_cleanup(&shell);
    return exit_status;
}
