#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "signals.h"
#include "jobs.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

volatile sig_atomic_t sigchld_received = 0;
volatile sig_atomic_t sigint_received = 0;

/* Async-signal-safe SIGCHLD handler */
static void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    sigchld_received = 1;
    /* We reap child status changes inside job_update_status() safely,
       or call waitpid here in a non-blocking loop */
    errno = saved_errno;
}

/* Async-signal-safe SIGINT handler */
static void sigint_handler(int sig) {
    (void)sig;
    sigint_received = 1;
    /* Output a fresh newline in raw mode or notify */
    const char msg[] = "\n";
    ssize_t w = write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    (void)w;
}

void init_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    /* SIGCHLD: catch to reap terminated/stopped child processes */
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction(SIGCHLD)");
    }

    /* SIGINT: shell catches Ctrl-C so it does not terminate */
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction(SIGINT)");
    }

    /* Interactive shell ignores SIGTSTP (Ctrl-Z), SIGQUIT, SIGTTIN, SIGTTOU */
    struct sigaction sa_ign;
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sa_ign.sa_flags = 0;

    sigaction(SIGTSTP, &sa_ign, NULL);
    sigaction(SIGQUIT, &sa_ign, NULL);
    sigaction(SIGTTIN, &sa_ign, NULL);
    sigaction(SIGTTOU, &sa_ign, NULL);
}

void reset_signals_for_child(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    /* Restore all signals to default disposition for children */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGTTIN, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
}

void block_sigchld(sigset_t *oldmask) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, oldmask);
}

void unblock_sigchld(const sigset_t *oldmask) {
    sigprocmask(SIG_SETMASK, oldmask, NULL);
}
