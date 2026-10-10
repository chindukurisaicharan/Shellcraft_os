# `rg` Round-Robin Scheduler

Shellcraft's `rg` built-in is a user-space Round-Robin scheduler for a set of external commands. It is separate from the shell's ordinary `jobs`/`fg`/`bg` job-control flow.

## Syntax

```text
rg [time-slice] program1 program2 [program3 ...]
rg [time-slice] -- command1 [args...] ::: command2 [args...]
```

The default time slice is `10ms`. Supported suffixes are `ms` and `s`, for example `rg 5ms ./a ./b` or `rg 1s ./a ./b`. In the shorthand form, each program token is an executable with no extra arguments. Use `:::` to delimit complete command/argument groups.

## Scheduling design

1. Parse the requested time slice and command list.
2. `fork()` one child per command. Each child creates its own process group, resets shell signal dispositions, and raises `SIGSTOP` before `execvp()` so no program starts until the scheduler selects it.
3. Wait until every successfully launched child is stopped.
4. In round-robin order, transfer the terminal to the selected child process group (interactive mode), send `SIGCONT`, and measure a slice against `CLOCK_MONOTONIC`.
5. At slice end, restore the shell terminal and send `SIGSTOP` to the selected process group. Wait for the leader to report stopped or exited before selecting another program, preventing two managed slices from overlapping.
6. Skip completed children. Repeat until all scheduled leaders exit. Ctrl+C/interruption triggers cleanup of managed children.

## Integration points

- `include/round_robin.h`, `src/round_robin.c`: scheduler implementation.
- `src/builtins.c`: registers `rg` as a built-in and dispatches to `builtin_rg()`.
- `Makefile`: compiles and links `src/round_robin.c`.
- `src/executor.c`: unchanged; it already dispatches direct built-ins in the shell process.
- `src/jobs.c` and `src/signals.c`: ordinary job management and signal setup remain unchanged. The scheduler uses its own wait loop for its own child PIDs, so it does not insert the managed children into the normal shell job table.

## Limitations

- This is a best-effort user-space time slicer, not a kernel scheduler. A 10 ms quantum is approximate, not a hard real-time deadline.
- It serializes the scheduled process groups; unrelated system processes can still run concurrently on other CPUs.
- The simple shorthand accepts executable names only; the `:::` form supports command arguments.
- Terminal-oriented programs may be awkward to time-slice because they expect continuous terminal ownership/input. Prefer CPU-bound or non-interactive test programs.
