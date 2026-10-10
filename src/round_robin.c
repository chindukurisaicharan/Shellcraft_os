#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "round_robin.h"
#include "shell.h"
#include "signals.h"
#include "terminal.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define RG_DEFAULT_SLICE_NS 10000000L
#define RG_MAX_PROGRAMS 64

typedef struct {
    pid_t pid;
    char *name;
    bool done;
    int wait_status;
    unsigned long slices;
} RgProgram;

static long long monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1;
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void sleep_for_slice(long long duration_ns) {
    long long start = monotonic_ns();
    if (start < 0) return;
    long long end = start + duration_ns;
    for (;;) {
        long long now = monotonic_ns();
        if (now < 0 || now >= end) break;
        long long left = end - now;
        struct timespec req = {
            .tv_sec = (time_t)(left / 1000000000LL),
            .tv_nsec = (long)(left % 1000000000LL)
        };
        while (nanosleep(&req, &req) < 0 && errno == EINTR) {
            /* Recompute the remaining time against CLOCK_MONOTONIC. */
            break;
        }
    }
}

static bool parse_slice(const char *s, long long *slice_ns) {
    if (!s || !slice_ns) return false;
    char *end = NULL;
    errno = 0;
    double value = strtod(s, &end);
    if (errno || end == s || value <= 0.0) return false;
    double multiplier;
    if (strcmp(end, "ms") == 0) multiplier = 1000000.0;
    else if (strcmp(end, "s") == 0) multiplier = 1000000000.0;
    else return false;
    double ns = value * multiplier;
    if (ns < 100000.0 || ns > 3600000000000.0) return false;
    *slice_ns = (long long)ns;
    return true;
}

static void child_exec(char **argv) {
    /* Each managed program gets its own process group for terminal control. */
    (void)setpgid(0, 0);
    reset_signals_for_child();
    /* Do not execute until the scheduler gives this process its first slice. */
    (void)raise(SIGSTOP);
    execvp(argv[0], argv);
    dprintf(STDERR_FILENO, "rg: %s: %s\n", argv[0], strerror(errno));
    _exit(errno == ENOENT ? 127 : 126);
}

static bool wait_until_stopped_or_done(RgProgram *p) {
    int status = 0;
    for (;;) {
        pid_t r = waitpid(p->pid, &status, WUNTRACED);
        if (r == p->pid) {
            p->wait_status = status;
            if (WIFSTOPPED(status)) return true;
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                p->done = true;
                return false;
            }
            continue;
        }
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && errno == ECHILD) {
            p->done = true;
            return false;
        }
        if (r < 0) {
            perror("rg: waitpid");
            p->done = true;
            return false;
        }
    }
}

static bool pause_current(RgProgram *p) {
    if (kill(-p->pid, SIGSTOP) < 0 && errno != ESRCH) {
        perror("rg: SIGSTOP");
    }
    return wait_until_stopped_or_done(p);
}

static void restore_shell_terminal(const struct ShellState *shell) {
    if (shell && shell->is_interactive && shell->shell_pgid > 0) {
        (void)tcsetpgrp(STDIN_FILENO, shell->shell_pgid);
    }
}

static void terminate_children(RgProgram *programs, int count, const struct ShellState *shell) {
    for (int i = 0; i < count; i++) {
        if (!programs[i].done && programs[i].pid > 0) (void)kill(-programs[i].pid, SIGTERM);
    }
    for (int i = 0; i < count; i++) {
        if (!programs[i].done && programs[i].pid > 0) {
            int status;
            while (waitpid(programs[i].pid, &status, 0) < 0 && errno == EINTR) {}
            programs[i].done = true;
        }
    }
    restore_shell_terminal(shell);
}

static void print_usage(void) {
    fprintf(stderr,
        "Usage:\n"
        "  rg [time-slice] program1 program2 [program3 ...]\n"
        "  rg [time-slice] -- command1 [args...] ::: command2 [args...]\n"
        "Examples:\n"
        "  rg ./cpu_a ./cpu_b\n"
        "  rg 10ms ./cpu_a ./cpu_b\n"
        "  rg 10ms -- ./worker 100 ::: ./worker 200\n"
        "Default time slice: 10ms. Scheduling continues until all programs exit.\n");
}

int builtin_rg(int argc, char **argv, struct ShellState *shell) {
    long long slice_ns = RG_DEFAULT_SLICE_NS;
    int first = 1;
    if (first < argc) {
        long long parsed;
        if (parse_slice(argv[first], &parsed)) {
            slice_ns = parsed;
            first++;
        }
    }
    if (first < argc && strcmp(argv[first], "--") == 0) first++;
    if (first >= argc) {
        print_usage();
        return 2;
    }

    bool grouped = false;
    for (int i = first; i < argc; i++) {
        if (strcmp(argv[i], ":::") == 0) grouped = true;
    }

    char **commands[RG_MAX_PROGRAMS];
    int count = 0;
    memset(commands, 0, sizeof(commands));

    if (!grouped) {
        for (int i = first; i < argc; i++) {
            if (count >= RG_MAX_PROGRAMS) {
                fprintf(stderr, "rg: maximum %d programs allowed\n", RG_MAX_PROGRAMS);
                return 2;
            }
            commands[count] = calloc(2, sizeof(char *));
            if (!commands[count]) { perror("rg: calloc"); goto parse_error; }
            commands[count][0] = argv[i];
            count++;
        }
    } else {
        int segment_start = first;
        for (int i = first; i <= argc; i++) {
            if (i == argc || strcmp(argv[i], ":::") == 0) {
                if (i == segment_start) {
                    fprintf(stderr, "rg: empty command around :::\n");
                    print_usage();
                    return 2;
                }
                if (count >= RG_MAX_PROGRAMS) {
                    fprintf(stderr, "rg: maximum %d programs allowed\n", RG_MAX_PROGRAMS);
                    return 2;
                }
                int n = i - segment_start;
                commands[count] = calloc((size_t)n + 1, sizeof(char *));
                if (!commands[count]) { perror("rg: calloc"); goto parse_error; }
                for (int a = 0; a < n; a++) commands[count][a] = argv[segment_start + a];
                count++;
                segment_start = i + 1;
            }
        }
    }

    sigint_received = 0;
    RgProgram programs[RG_MAX_PROGRAMS];
    memset(programs, 0, sizeof(programs));
    int launched = 0;

    /* Start every child and wait until each has stopped before scheduling. */
    for (int i = 0; i < count; i++) {
        pid_t pid = fork();
        if (pid == 0) child_exec(commands[i]);
        if (pid < 0) {
            perror("rg: fork");
            terminate_children(programs, launched, shell);
            for (int k = 0; k < count; k++) free(commands[k]);
            return 1;
        }
        programs[i].pid = pid;
        programs[i].name = commands[i][0];
        (void)setpgid(pid, pid);
        launched++;
        if (!wait_until_stopped_or_done(&programs[i])) {
            /* It may have failed before its first scheduled slice. */
            fprintf(stderr, "rg: %s exited before its first time slice\n", programs[i].name);
        }
    }

    printf("rg: Round-Robin scheduler started: %d program(s), slice=%g ms\n",
           count, (double)slice_ns / 1000000.0);
    fflush(stdout);

    int remaining = 0;
    for (int i = 0; i < count; i++) if (!programs[i].done) remaining++;
    int last_status = 0;
    unsigned long long switches = 0;
    bool interrupted = false;

    while (remaining > 0) {
        for (int i = 0; i < count && remaining > 0; i++) {
            RgProgram *p = &programs[i];
            if (p->done) continue;

            if (sigint_received) {
                interrupted = true;
                break;
            }

            if (shell && shell->is_interactive) (void)tcsetpgrp(STDIN_FILENO, p->pid);
            if (kill(-p->pid, SIGCONT) < 0) {
                if (errno == ESRCH) {
                    int status;
                    pid_t wr = waitpid(p->pid, &status, WNOHANG);
                    if (wr == p->pid) p->wait_status = status;
                    p->done = true;
                    remaining--;
                    restore_shell_terminal(shell);
                    continue;
                }
                perror("rg: SIGCONT");
                restore_shell_terminal(shell);
                continue;
            }

            p->slices++;
            switches++;
            sleep_for_slice(slice_ns);
            restore_shell_terminal(shell);

            if (pause_current(p)) {
                /* The process is now stopped and cannot overlap the next slice. */
            } else {
                remaining--;
                int program_status = 0;
                if (WIFEXITED(p->wait_status)) program_status = WEXITSTATUS(p->wait_status);
                else if (WIFSIGNALED(p->wait_status)) program_status = 128 + WTERMSIG(p->wait_status);
                last_status = program_status;
                printf("rg: %s finished (status=%d, slices=%lu); remaining=%d\n",
                       p->name, program_status, p->slices, remaining);
                fflush(stdout);
            }
        }
        if (interrupted) break;
    }

    if (interrupted) {
        fprintf(stderr, "\nrg: interrupted; terminating managed programs\n");
        terminate_children(programs, count, shell);
        return 130;
    }

    restore_shell_terminal(shell);
    printf("rg: all programs finished; total time slices/context switches=%llu\n", switches);
    for (int k = 0; k < count; k++) free(commands[k]);
    return last_status;

parse_error:
    for (int k = 0; k < RG_MAX_PROGRAMS; k++) free(commands[k]);
    return 2;
}
