# POSIX System Call Mapping in Shellcraft

This reference document maps every POSIX system call utilized within **Shellcraft** to its corresponding subsystem, invocation site, kernel mechanics, and error-handling policy.

---

## 1. Master System Call Matrix

| System Call | Header | Shellcraft Subsystem / File | Purpose & Role |
| :--- | :--- | :--- | :--- |
| `fork()` | `<unistd.h>` | [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c), [`src/expansion.c`](file:///d:/New%20folder%20(4)/src/expansion.c) | Creates a duplicate child process with copy-on-write virtual address space. |
| `execvp()` | `<unistd.h>` | [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c) | Replaces process address space with executable binary, searching `$PATH`. |
| `_exit()` | `<unistd.h>` | [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c), [`src/expansion.c`](file:///d:/New%20folder%20(4)/src/expansion.c) | Immediate process termination without running user-space `atexit` handlers or flushing stdio buffers. |
| `waitpid()` | `<sys/wait.h>` | [`src/jobs.c`](file:///d:/New%20folder%20(4)/src/jobs.c), [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c) | Waits for state changes in child processes (`WNOHANG`, `WUNTRACED`, `WCONTINUED`). |
| `setpgid()` | `<unistd.h>` | [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c), [`src/jobs.c`](file:///d:/New%20folder%20(4)/src/jobs.c) | Establishes or joins a process group for job-control signal arbitration. |
| `getpgrp()` | `<unistd.h>` | [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c) | Retrieves the process group ID of the caller. |
| `tcgetpgrp()` | `<unistd.h>` | [`src/shell.c`](file:///d:/New%20folder%20(4)/src/shell.c), [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c) | Queries the foreground process group associated with the controlling terminal. |
| `tcsetpgrp()` | `<unistd.h>` | [`src/jobs.c`](file:///d:/New%20folder%20(4)/src/jobs.c), [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c) | Assigns terminal foreground control to a specific process group. |
| `pipe()` | `<unistd.h>` | [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c), [`src/expansion.c`](file:///d:/New%20folder%20(4)/src/expansion.c) | Allocates unidirectional kernel FIFO buffer for inter-process communication. |
| `open()` | `<fcntl.h>` | [`src/redirection.c`](file:///d:/New%20folder%20(4)/src/redirection.c), [`src/expansion.c`](file:///d:/New%20folder%20(4)/src/expansion.c) | Opens file descriptors for input (`<`), output (`>`), and append (`>>`). |
| `close()` | `<unistd.h>` | [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c), [`src/redirection.c`](file:///d:/New%20folder%20(4)/src/redirection.c) | Closes unused file descriptors, freeing file table entries and signaling EOF on pipes. |
| `dup()` | `<unistd.h>` | [`src/redirection.c`](file:///d:/New%20folder%20(4)/src/redirection.c) | Duplicates standard file descriptors (0, 1, 2) before in-process builtin execution. |
| `dup2()` | `<unistd.h>` | [`src/redirection.c`](file:///d:/New%20folder%20(4)/src/redirection.c), [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c) | Atomically clones a descriptor onto a specific target slot (e.g. STDOUT_FILENO). |
| `read()` | `<unistd.h>` | [`src/terminal.c`](file:///d:/New%20folder%20(4)/src/terminal.c), [`src/expansion.c`](file:///d:/New%20folder%20(4)/src/expansion.c) | Synchronously reads raw bytes from terminal or command substitution pipe. |
| `write()` | `<unistd.h>` | [`src/signals.c`](file:///d:/New%20folder%20(4)/src/signals.c) | Async-signal-safe output inside signal handlers. |
| `sigaction()` | `<signal.h>` | [`src/signals.c`](file:///d:/New%20folder%20(4)/src/signals.c) | Registers reliable POSIX signal handlers (`SA_RESTART`, `SA_SIGINFO`). |
| `sigprocmask()` | `<signal.h>` | [`src/signals.c`](file:///d:/New%20folder%20(4)/src/signals.c), [`src/executor.c`](file:///d:/New%20folder%20(4)/src/executor.c) | Atomically inspects and updates the calling thread's blocked signal mask. |
| `kill()` | `<signal.h>` | [`src/jobs.c`](file:///d:/New%20folder%20(4)/src/jobs.c) | Transmits signals (`SIGCONT`, `SIGTERM`, `SIGKILL`) to processes or process groups (`-pgid`). |
| `tcgetattr()` | `<termios.h>` | [`src/terminal.c`](file:///d:/New%20folder%20(4)/src/terminal.c), [`src/jobs.c`](file:///d:/New%20folder%20(4)/src/jobs.c) | Retrieves current terminal driver parameters (baud, flags, special characters). |
| `tcsetattr()` | `<termios.h>` | [`src/terminal.c`](file:///d:/New%20folder%20(4)/src/terminal.c), [`src/jobs.c`](file:///d:/New%20folder%20(4)/src/jobs.c) | Configures raw mode or restores original canonical mode parameters. |
| `isatty()` | `<unistd.h>` | [`src/terminal.c`](file:///d:/New%20folder%20(4)/src/terminal.c), [`src/shell.c`](file:///d:/New%20folder%20(4)/src/shell.c) | Determines if standard file descriptors refer to a valid terminal. |
| `chdir()` | `<unistd.h>` | [`src/builtins.c`](file:///d:/New%20folder%20(4)/src/builtins.c) | Changes process current working directory table pointer in the Linux kernel VFS. |
| `getcwd()` | `<unistd.h>` | [`src/builtins.c`](file:///d:/New%20folder%20(4)/src/builtins.c), [`src/shell.c`](file:///d:/New%20folder%20(4)/src/shell.c) | Resolves absolute path of working directory by traversing dentry parent pointers. |
| `glob()` | `<glob.h>` | [`src/expansion.c`](file:///d:/New%20folder%20(4)/src/expansion.c) | Searches filesystem paths matching wildcards (`*`, `?`, `[...]`). |
| `globfree()` | `<glob.h>` | [`src/expansion.c`](file:///d:/New%20folder%20(4)/src/expansion.c) | Deallocates path matching buffers allocated by `glob()`. |

---

## 2. In-Depth Kernel Mechanics

### 2.1 `fork()` and Copy-on-Write (COW)
- **Kernel Mechanism**: When Shellcraft calls `fork()`, the Linux kernel duplicates the task structure (`task_struct`), allocates new thread info, and marks all virtual memory pages in the page table as read-only.
- **Copy-on-Write**: Physical pages are shared between parent and child until one process attempts a write. A page fault triggers the kernel MMU to allocate a private physical page replica.
- **Why this matters for Shellcraft**: Allows subshells and pipelines to be launched with low overhead (~100 microseconds).

### 2.2 `dup2(oldfd, newfd)` File Descriptor Rewiring
- **Kernel Mechanism**:
  1. Checks if `newfd` is open; if so, atomically closes it.
  2. Increments the reference count of the file struct (`struct file`) pointed to by `oldfd`.
  3. Updates entry `newfd` in the process descriptor table (`files_struct->fd_array`) to point to the same `struct file`.
- **Why this matters for Shellcraft**: Guarantees that redirected output (`> file`) or pipe inputs appear seamlessly on standard slots (0, 1, 2) prior to `execvp()`.

### 2.3 `pipe(int pipefd[2])` Inter-Process Data Streaming
- **Kernel Mechanism**:
  1. Allocates a circular memory ring buffer in the Linux kernel (default capacity 65,536 bytes).
  2. Allocates two `struct file` instances: one write-only (`pipefd[1]`), one read-only (`pipefd[0]`).
  3. When writers write, data is appended to the ring buffer. When readers read, data is drained.
  4. If the ring buffer fills up, writing blocks until space is available.
  5. **EOF Condition**: When all file descriptors pointing to `pipefd[1]` are closed, subsequent reads on `pipefd[0]` return 0 bytes (EOF). This is why Shellcraft strictly closes all unused pipe ends in both child and parent processes.
