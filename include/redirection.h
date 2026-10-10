#ifndef SHELLCRAFT_REDIRECTION_H
#define SHELLCRAFT_REDIRECTION_H

#include "ast.h"

/* Applies all redirections in order. Returns 0 on success, -1 on error. */
int apply_redirections(const Redirection *redirs);

/* Saves standard fds (stdin=0, stdout=1, stderr=2) using dup() for builtin redirection restoration */
int save_standard_fds(int saved_fds[3]);

/* Restores standard fds using dup2() and closes the saved copies */
void restore_standard_fds(int saved_fds[3]);

#endif /* SHELLCRAFT_REDIRECTION_H */
