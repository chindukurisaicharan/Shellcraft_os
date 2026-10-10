# Shellcraft System Design Document

**Project:** Shellcraft — The Command Line, Rebuilt  
**Standard:** C11 / POSIX.1-2008  
**Domain:** Systems Programming, Operating Systems, Concurrency, IPC  

---

## 1. Architectural Philosophy & Design Goals

The design of Shellcraft is governed by strict systems-programming principles:
1. **Zero Shortcuts**: Absolutely no high-level standard library cheating (`system()`, `popen()`, `wordexp()`, or invoking `/bin/sh`). Every fork, exec, pipe, redirection, and signal must use fundamental Linux kernel system calls.
2. **Explicit Lifecycles**: All memory allocated during parsing, tokenization, and expansion must have strictly defined ownership semantics and be explicitly freed without residual memory leaks.
3. **Robust Concurrency & Race Freedom**: Critical sections where asynchronous signal delivery (`SIGCHLD`) intersects with state modification (inserting jobs into the active job table) must be protected using atomic signal masks (`sigprocmask`).
4. **Clean Abstractions**: Each subsystem (tokenizer, parser, AST, executor, job controller, signal multiplexer, terminal line editor) operates behind well-defined C header interfaces.

---

## 2. Component Pipeline & Data Flow

```
                      Raw Terminal / Stdin
                               │
                               ▼
                       Line String (char*)
                               │
                               ▼
                        [Tokenizer] 
                   (DFA Lexer State Machine)
                               │
                               ▼
                         Token Stream 
                    (Array of Token structs)
                               │
                               ▼
                         [Parser] 
                 (Recursive Descent Grammar)
                               │
                               ▼
                    [Abstract Syntax Tree]
                    AstProgram -> AstConditional
                               -> AstPipeline
                               -> AstCommand
                               │
                               ▼
                        [Expansion]
                 ($VAR, ~, *, $(subcmd))
                               │
                               ▼
                        [Executor]
                ┌──────────────┼──────────────┐
                ▼              ▼              ▼
           Built-ins      Single Exec     Pipeline
          (in-process)    (fork/exec)   (N-fork/pipes)
                │              │              │
                └──────────────┼──────────────┘
                               │
                               ▼
                       [Job Controller]
                (Process Groups, tcsetpgrp,
                 waitpid, Terminal Modes)
```

---

## 3. Core Data Structures

### 3.1 Tokenizer (`include/tokenizer.h`)

The lexical scanner breaks raw input into strongly typed tokens:

```c
typedef enum {
    TOKEN_WORD,           /* Normal argument or command name */
    TOKEN_PIPE,           /* | */
    TOKEN_AND,            /* && */
    TOKEN_OR,             /* || */
    TOKEN_SEMICOLON,      /* ; */
    TOKEN_AMPERSAND,      /* & */
    TOKEN_REDIRECT_IN,    /* < */
    TOKEN_REDIRECT_OUT,   /* > */
    TOKEN_REDIRECT_APPEND,/* >> */
    TOKEN_REDIRECT_ERR,   /* 2> */
    TOKEN_REDIRECT_ERR_APPEND, /* 2>> */
    TOKEN_REDIRECT_ERR_OUT,    /* 2>&1 */
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *value;
    int line;
    int col;
} Token;
```

### 3.2 Abstract Syntax Tree (`include/ast.h`)

The grammar is expressed as an explicit hierarchical tree:

```c
typedef struct AstRedirection {
    RedirectionType type;
    char *filename;
    int fd;
    struct AstRedirection *next;
} AstRedirection;

typedef struct AstCommand {
    char **argv;
    int argc;
    AstRedirection *redirections;
} AstCommand;

typedef struct AstPipeline {
    AstCommand **commands;
    int command_count;
    bool background;
} AstPipeline;

typedef struct AstConditional {
    AstPipeline *pipeline;
    ConditionalOp op;      /* OP_NONE, OP_AND, OP_OR */
    struct AstConditional *next;
} AstConditional;

typedef struct AstProgram {
    AstConditional **conditionals;
    int conditional_count;
} AstProgram;
```

### 3.3 Job & Process Management (`include/jobs.h`)

Tracks concurrent processes within process groups:

```c
typedef enum {
    PROCESS_RUNNING,
    PROCESS_STOPPED,
    PROCESS_COMPLETED
} ProcessState;

typedef struct Process {
    pid_t pid;
    char **argv;
    int argc;
    ProcessState state;
    int status;
    struct Process *next;
} Process;

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobState;

typedef struct Job {
    int id;               /* 1-based job identifier */
    pid_t pgid;           /* Process Group ID */
    char *command_line;   /* Representation string */
    Process *first_process;
    JobState state;
    bool notified;
    struct termios tmodes;/* Saved terminal state when stopped */
    struct Job *next;
} Job;
```

---

## 4. Key Subsystem Implementation Details

### 4.1 Recursive-Descent Parsing
The grammar precedence is implemented using traditional predictive recursive descent:
1. `parse_program`: Parses commands separated by `;` or `\n`.
2. `parse_conditional`: Parses pipelines joined by `&&` and `||` (left-associative).
3. `parse_pipeline`: Parses commands connected by `|` and checks for trailing `&`.
4. `parse_command`: Collects argument words and interleaving redirection operators (`<`, `>`, `>>`, `2>`, `2>&1`).

If an unexpected token or syntax error occurs (e.g. `ls | | grep foo`), the parser halts, emits a clear error with column index, and frees partially allocated nodes.

### 4.2 Safe Execution & Process Group Arbitration
When executing commands:
- **Foreground External Commands**:
  1. The child calls `setpgid(0, 0)` to place itself into its own process group.
  2. The parent calls `setpgid(pid, pid)` immediately after fork to eliminate the race condition where parent or child proceeds before PGID initialization.
  3. The shell assigns terminal ownership to the child's process group using `tcsetpgrp(STDIN_FILENO, pgid)`.
  4. The shell waits for all processes in the job using `waitpid(-pgid, &status, WUNTRACED)`.
  5. When the job exits or suspends (`WIFSTOPPED`), the shell reclaims terminal control via `tcsetpgrp(STDIN_FILENO, shell_pgid)` and restores its raw terminal settings.
- **Background Commands (`&`)**:
  1. The process group is created via `setpgid()`, but `tcsetpgrp()` is NOT called.
  2. Terminal control remains with the shell.
  3. The shell does not block; it registers the job in the active job table and immediately prints `[job_id] pid`.

### 4.3 Pipeline Execution ($N$-stage)
For an $N$-stage pipeline:
1. $N-1$ pipes are allocated via `pipe(pipes[i])`.
2. $N$ child processes are forked in sequence.
3. In child $i$:
   - If $i > 0$: `dup2(pipes[i-1][0], STDIN_FILENO)`.
   - If $i < N-1$: `dup2(pipes[i][1], STDOUT_FILENO)`.
   - **Crucial Invariant**: All $2(N-1)$ pipe file descriptor ends are explicitly closed in the child.
   - Command-specific file redirections are applied (overriding pipe ends if specified).
   - Execution proceeds via `execvp()`.
4. In the parent:
   - All $2(N-1)$ pipe ends are closed immediately after forking all stages. This ensures that EOF is properly sent through the pipeline when writers terminate.
   - If foreground, the shell waits for all child PIDs. The exit status of the rightmost stage is returned as `$?`.

### 4.4 Redirection Management
Redirections can occur anywhere within a command.
- File descriptor cloning is performed using `dup2(fd, target_fd)`.
- For built-in commands executing in the parent process (e.g., `cd`, `echo > file`), original descriptors are saved via `dup(STDIN_FILENO)`, `dup(STDOUT_FILENO)`, `dup(STDERR_FILENO)`. After execution, they are restored via `dup2()` and the clones are closed.
- `2>&1` uses `dup2(STDOUT_FILENO, STDERR_FILENO)`.

### 4.5 Asynchronous Signal Handling & Reaping
- `SIGINT` (`Ctrl-C`): Caught by `sigint_handler`. If foreground child is running, signal is delivered to child's process group. In interactive prompt mode, prints newline and displays fresh prompt.
- `SIGTSTP` (`Ctrl-Z`): Sent by terminal driver to foreground process group. Suspends processes; job controller marks job as `STOPPED` and prints job status.
- `SIGCHLD`: Notifies shell of child state changes.
  - To prevent race conditions during `fork()` and `job_add()`, `SIGCHLD` is temporarily blocked using `sigprocmask(SIG_BLOCK, &set, &old)`.
  - Once the job is linked into the job table, `sigprocmask(SIG_SETMASK, &old, NULL)` restores the mask.
  - Reaping is handled via `job_reap_zombies()`, invoking non-blocking `waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)`.

---

## 5. Memory Management Strategy

1. **Defensive Wrappers**: All heap allocations use `safe_malloc()`, `safe_calloc()`, `safe_realloc()`, and `safe_strdup()` which check for NULL and abort gracefully on out-of-memory.
2. **Grammar Tree Deconstruction**:
   - `ast_free_program(AstProgram *prog)` recursively traverses and frees all `AstConditional`, `AstPipeline`, `AstCommand`, `argv` vectors, and `AstRedirection` chains.
3. **Token Stream Cleanup**: `tokenizer_free_tokens(Token *tokens, int count)` frees all scanned string values and the token buffer.
4. **Environment Buffers**: Expanded arguments are copied into fresh dynamically allocated arrays and released after `execvp` or built-in dispatch via `free_str_array()`.
5. **Leak-Free Verified**: Verified with Valgrind (`--leak-check=full --show-leak-kinds=all --track-fds=yes`) with 0 bytes lost.
