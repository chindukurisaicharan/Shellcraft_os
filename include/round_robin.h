#ifndef SHELLCRAFT_ROUND_ROBIN_H
#define SHELLCRAFT_ROUND_ROBIN_H

struct ShellState;

/* Run programs one at a time in 10 ms round-robin slices. */
int builtin_rg(int argc, char **argv, struct ShellState *shell);

#endif
