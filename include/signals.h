#ifndef SHELLCRAFT_SIGNALS_H
#define SHELLCRAFT_SIGNALS_H

#include <signal.h>
#include <stdbool.h>

/* Global async-signal-safe flags */
extern volatile sig_atomic_t sigchld_received;
extern volatile sig_atomic_t sigint_received;

/* Setup signal handlers for interactive shell */
void init_signals(void);

/* Reset signals to default in child process before exec */
void reset_signals_for_child(void);

/* Signal blocking utilities to prevent race conditions during job table modifications */
void block_sigchld(sigset_t *oldmask);
void unblock_sigchld(const sigset_t *oldmask);

#endif /* SHELLCRAFT_SIGNALS_H */
