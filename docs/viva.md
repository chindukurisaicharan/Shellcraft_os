# Shellcraft: Operating Systems Viva Voce & Defense Guide

This document prepares project authors for defense examinations, viva voce evaluations, and technical questioning on operating systems and systems programming concepts demonstrated in **Shellcraft**.

---

## 1. Process Management & Execution

### Q1: What is the fundamental difference between `fork()` and `execvp()`?
- **`fork()`**: Creates a new child process by duplicating the calling process's virtual memory address space (using Copy-on-Write), file descriptor table, signal dispositions, and credentials. The child receives a unique PID and PPID.
- **`execvp()`**: Replaces the calling process's current memory image (code, stack, heap, data segments) with a new executable program loaded from the filesystem, retaining the same PID and open file descriptors (unless `O_CLOEXEC` is set).
- **In Shellcraft**: `fork()` generates the process context; `execvp()` loads the user command inside the child.

---

### Q2: Why MUST built-in commands like `cd` and `export` be executed in the shell's process rather than forked?
- When a process calls `chdir()` or updates its environment table via `setenv()`, the modifications apply **exclusively to that process's address space** and kernel task struct.
- If the shell forked a child to execute `cd /tmp`, the child would change its working directory and then immediately terminate. The parent shell's working directory would remain unchanged.
- Therefore, Shellcraft executes `cd`, `export`, `alias`, and `exit` directly inside the parent shell process.

---

### Q3: What is a Zombie process, why is it dangerous, and how does Shellcraft prevent them?
- **Definition**: A zombie process is a terminated process whose kernel task struct (`task_struct`) and exit status remain in the operating system process table because its parent has not yet called `wait()` or `waitpid()`.
- **Danger**: While zombies consume minimal memory, they tie up process IDs (`PID`). If zombies accumulate, the system exhausts its PID allocation limit (`/proc/sys/kernel/pid_max`), preventing any new processes from spawning.
- **Prevention in Shellcraft**: Shellcraft implements non-blocking reaping via `job_reap_zombies()` using:
  ```c
  waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
  ```
  This is executed whenever `SIGCHLD` arrives and before each interactive prompt, immediately harvesting terminated children. In automated testing with 300 concurrent background processes, Shellcraft left **0 zombies**.

---

### Q4: What is an Orphan process, and what happens to it when its parent terminates?
- An orphan process is a running child process whose parent terminates before the child.
- **Kernel Mechanism**: In modern Linux, orphaned processes are adopted by `init` (PID 1) or a subreaper process (like `systemd --user`). The adoptive parent periodically calls `wait()` to ensure that when the orphan exits, it does not remain a zombie.

---

### Q5: What is Copy-on-Write (COW) and how does Linux optimize `fork()`?
- Historically, `fork()` physically copied all physical memory pages from parent to child, which was slow and memory-intensive.
- Modern Linux uses **Copy-on-Write**: `fork()` duplicates the parent's page tables and marks all pages as read-only. Both parent and child read from the same physical pages.
- When either process attempts to write to a page, a CPU page fault occurs. The kernel catches the fault, allocates a new physical frame, copies the 4KB page, and updates that process's page table with write permissions.
- Because shells usually call `execvp()` immediately after `fork()`, COW prevents unnecessary memory duplication.

---

### Q6: Why MUST forked child processes terminate using `_exit()` rather than `exit()`?
- `exit()` is a high-level C library function that runs registered `atexit()` handlers, flushes user-space stdio streams (`FILE*`), and can rewind shared file descriptors.
- `_exit()` (or `_Exit()`) is a direct Linux kernel system call that immediately terminates the process without triggering user-space stdio cleanups.
- **Critical Shell Bug Prevented**: If a child calls `exit()`, it triggers the parent's `atexit(terminal_restore)` handler and flushes buffered standard I/O (e.g., rewind buffered `stdin` in scripts), corrupting subsequent commands. Shellcraft strictly enforces `_exit()` in all child branches.

---

## 2. Job Control & Terminal Arbitration

### Q7: What is a Process Group (PGRP) and why is it needed in a shell?
- A Process Group is a collection of related processes sharing a common Process Group ID (`PGID`).
- **Need in Shells**: A single command line may consist of a pipeline with multiple processes (e.g. `cat file | grep pattern | wc -l`). When the user presses `Ctrl-C` or `Ctrl-Z`, the signal must be delivered to **all** processes in the pipeline simultaneously. By placing them in the same process group, the kernel can signal the entire group using `kill(-pgid, signum)`.

---

### Q8: How does `tcsetpgrp()` arbitrate access to the terminal?
- The controlling terminal (`/dev/pts/X`) only grants input access and generates keyboard signals for the single **foreground process group**.
- When launching a foreground command, Shellcraft calls `tcsetpgrp(STDIN_FILENO, pgid)` to hand off terminal ownership to the child's process group.
- When the job completes or suspends, Shellcraft reclaims the terminal by invoking `tcsetpgrp(STDIN_FILENO, shell_pgid)`.

---

### Q9: What causes `SIGTTIN` and `SIGTTOU`?
- **`SIGTTIN`**: Generated by the terminal driver when a **background** process group attempts to read from standard input (`/dev/tty`). The kernel sends `SIGTTIN` to suspend the offending process group until brought to the foreground.
- **`SIGTTOU`**: Generated when a background process attempts to write to the terminal (if `TOSTOP` flag is set) or attempts to alter terminal settings (`tcsetattr`, `tcsetpgrp`).
- Shellcraft ignores `SIGTTOU` and `SIGTTIN` in the shell process to prevent itself from being suspended during terminal hand-offs.

---

### Q10: What is the race condition between `fork()`, `setpgid()`, and `SIGCHLD`, and how is it solved?
- **The Race**: If only the child calls `setpgid()`, the parent might call `tcsetpgrp()` or `waitpid()` before the child executes `setpgid()`. If only the parent calls `setpgid()`, the child might execute `execvp()` before the parent's call completes (POSIX causes `setpgid()` to fail with `EACCES` after `execve`). Furthermore, if the child terminates before the parent adds it to the job table, `SIGCHLD` could arrive and reap a PID that isn't registered yet.
- **Shellcraft's Solution**:
  1. Blocks `SIGCHLD` via `sigprocmask()` before `fork()`.
  2. Calls `setpgid()` in **both** parent and child.
  3. Registers the child into the job table.
  4. Unblocks `SIGCHLD` via `sigprocmask()`.

---

### Q11: Explain the job state transitions between `RUNNING`, `STOPPED`, and `TERMINATED`.
- A job starts in `RUNNING_FOREGROUND` or `RUNNING_BACKGROUND`.
- When the user presses `Ctrl-Z`, the kernel sends `SIGTSTP` to the foreground process group. The processes stop, and `waitpid()` with `WUNTRACED` informs the shell. The job transitions to `STOPPED`.
- The user can resume it in the background via `bg` (which sends `kill(-pgid, SIGCONT)`), transitioning it to `RUNNING_BACKGROUND`.
- Or the user can bring it to the foreground via `fg` (which calls `tcsetpgrp()`, sends `SIGCONT`, and waits), transitioning it to `RUNNING_FOREGROUND`.
- When all processes exit, `job_reap_zombies()` marks the job as `TERMINATED` and removes it from the table.

---

## 3. Inter-Process Communication & File Descriptors

### Q12: How does Shellcraft implement arbitrary $N$-stage pipelines?
- For an $N$-stage pipeline, Shellcraft allocates $N-1$ pipes via `pipe()`.
- It forks $N$ child processes in sequence:
  - Child 0: Redirects stdout to `pipes[0][1]`.
  - Child $i$ ($0 < i < N-1$): Redirects stdin from `pipes[i-1][0]` and stdout to `pipes[i][1]`.
  - Child $N-1$: Redirects stdin from `pipes[N-2][0]`.
- **Crucial Step**: Every child closes all $2(N-1)$ pipe file descriptors after performing its specific `dup2()` assignments.
- The parent closes all $2(N-1)$ pipe descriptors immediately after spawning all stages.

---

### Q13: Why MUST unused pipe ends be closed in BOTH parent and child?
- In Linux, a reading process on a pipe receives End-Of-File (EOF, 0 bytes) only when **all open file descriptors referring to the write end of that pipe across all processes are closed**.
- If the parent or any child forgets to close a write descriptor `pipes[i][1]`, the reader process `pipes[i][0]` will hang indefinitely waiting for more data, causing the entire pipeline to freeze.

---

### Q14: How does `dup2(oldfd, newfd)` work in the kernel?
- The Linux kernel maintains three tables:
  1. Per-process file descriptor table.
  2. System-wide open file description table (contains file offset, status flags, and refcount).
  3. Inode table.
- When `dup2(oldfd, newfd)` is called:
  - If `newfd` is currently open, it is atomically closed.
  - Slot `newfd` in the process descriptor table is modified to point to the same open file description as `oldfd`.
  - The reference count of that open file description is incremented.

---

### Q15: How does `2>&1` redirection work?
- `2` represents `STDERR_FILENO`, `1` represents `STDOUT_FILENO`.
- The syntax `2>&1` means: "Make standard error refer to wherever standard output is currently pointing."
- Shellcraft implements this with:
  ```c
  dup2(STDOUT_FILENO, STDERR_FILENO);
  ```
- If standard output was previously redirected to a file (`> log.txt`), standard error will now write into the same open file description at the same shared offset.

---

## 4. Signal Handling & Asynchronous Mechanics

### Q16: What is an async-signal-safe function, and why can't `printf()` or `malloc()` be called in a signal handler?
- An async-signal-safe function can be safely interrupted and re-entered by a signal handler without causing race conditions or deadlocks.
- Functions like `malloc()`, `free()`, and `printf()` maintain internal state protected by mutexes or non-reentrant data structures. If a signal occurs while `malloc()` is holding a lock and the signal handler also calls `malloc()`, the process will permanently deadlock.
- **Shellcraft Compliance**: Handlers only touch `volatile sig_atomic_t` flags and call async-safe `write()`.

---

### Q17: How does Shellcraft handle `Ctrl-C` (`SIGINT`) without terminating itself?
- Shellcraft installs a custom signal action via `sigaction(SIGINT, &sa, NULL)`.
- When running in interactive mode, the shell process catches `SIGINT`. It prints a newline and redraws an empty prompt without terminating.
- When a foreground child is running, terminal ownership has been transferred to the child's process group (`tcsetpgrp`). The kernel delivers `SIGINT` directly to the child's group, killing the child (unless caught by the child), while the shell's handler updates `$? = 130` upon reaping.

---

### Q18: What is the purpose of `SA_RESTART` in `sigaction`?
- By default in Unix, if a process is blocked in a slow system call (e.g. `read()`, `waitpid()`) when a signal arrives, the system call fails and returns `-1` with `errno = EINTR`.
- Setting the `SA_RESTART` flag in `struct sigaction.sa_flags` directs the Linux kernel to automatically restart interrupted system calls once the signal handler returns.

---

## 5. Lexing, Parsing & System Architecture

### Q19: Why use an Abstract Syntax Tree (AST) instead of naive string splitting (`strtok`)?
- Naive string splitting cannot handle complex grammar constructs:
  - Nested command substitutions: `echo $(ls | grep foo)`
  - Correct operator precedence: `cmd1 && cmd2 || cmd3` vs `cmd1 | cmd2 &`
  - Quoting semantics: `echo "a | b && c"` (pipes and ampersands must be treated as literal characters, not operators).
- Shellcraft's tokenizer and recursive-descent parser decouple syntactic analysis from execution, creating a validated tree representation that guarantees correct precedence and execution order.

---

### Q20: How does command substitution `$(command)` work?
1. The expansion engine scans for `$(...)` and extracts the inner command string.
2. It allocates a pipe (`pipe(pipefd)`).
3. It forks an isolated subshell child process.
4. The child redirects its standard output to `pipefd[1]`, redirects its standard input to `/dev/null` (preventing interference with parent script input), and executes the inner command.
5. The parent reads stdout from `pipefd[0]`, strips trailing newlines, and replaces the `$(...)` token with the captured string.

---

### Q21: What is the difference between Canonical Mode and Raw Mode in terminal I/O?
- **Canonical Mode (Cooked)**: Input is processed line-by-line. The terminal driver buffers keystrokes until the user presses Enter, handling basic backspace editing locally.
- **Raw Mode**: Keystrokes are delivered to the application byte-by-byte immediately (`VMIN=1, VTIME=0`). Echo is disabled, allowing the application to intercept special keys (`Ctrl-A`, `Ctrl-E`, Up, Down, Backspace) to implement custom line editing and command history navigation.

---

### Q22: What is the role of `sigprocmask()` in Shellcraft?
- `sigprocmask()` allows a process to selectively examine or change its signal mask (the set of signals currently blocked from delivery).
- Shellcraft uses `sigprocmask(SIG_BLOCK, &mask, &oldmask)` to create critical sections during `fork()` and job creation. This guarantees that `SIGCHLD` signals cannot interrupt the shell until the child process is safely entered into the job control table.

---

### Q23: How does Shellcraft implement conditional short-circuiting (`&&`, `||`)?
- In `execute_program()`, expressions are evaluated from left to right:
  - For `cmd1 && cmd2`: `cmd2` is evaluated **only if** `cmd1` exited with status 0.
  - For `cmd1 || cmd2`: `cmd2` is evaluated **only if** `cmd1` exited with non-zero status.
- If the condition is false, execution skips the following pipeline without invoking `fork()`.

---

### Q24: How was memory leak freedom and file descriptor hygiene verified?
- Shellcraft was verified using **Valgrind 3.22.0** with strict parameters:
  ```bash
  valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./shellcraft
  ```
- **Results**:
  - `definitely lost: 0 bytes in 0 blocks`
  - `indirectly lost: 0 bytes in 0 blocks`
  - `possibly lost: 0 bytes in 0 blocks`
  - `ERROR SUMMARY: 0 errors from 0 contexts`
  - File descriptors 0, 1, and 2 only remained open at exit.
- Additionally tested with GCC **AddressSanitizer** and **UndefinedBehaviorSanitizer** (`make sanitize`).

---

### Q25: If you had to add job control support for background output suspension (`TOSTOP`), how would you do it?
- You would query the current terminal attributes with `tcgetattr(STDIN_FILENO, &tmodes)`, set the `TOSTOP` bit in `tmodes.c_lflag |= TOSTOP`, and apply it with `tcsetattr(STDIN_FILENO, TCSANOW, &tmodes)`.
- With `TOSTOP` active, any background process attempting to write to standard output would be caught by the kernel terminal driver and sent `SIGTTOU`, suspending it until the user brought it to the foreground with `fg`.
