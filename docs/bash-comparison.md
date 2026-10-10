# Shellcraft vs. GNU Bash: Architectural Comparison

This document contrasts the architecture, design choices, and systems-level tradeoffs of **Shellcraft** with **GNU Bash** (Bourne Again Shell).

---

## 1. Feature & Capability Matrix

| Feature / Metric | GNU Bash (v5.2) | Shellcraft (v1.0) |
| :--- | :--- | :--- |
| **Language & Standard** | C89 / C99 + GNU extensions | C11 strictly conforming to POSIX.1-2008 |
| **External Dependencies** | GNU Readline, Ncurses, Gettext, Bison | **Zero external dependencies** (Standard C Library only) |
| **Parser Type** | LALR(1) generated via Bison (`parse.y`) | Hand-crafted Recursive-Descent with explicit AST |
| **Terminal Line Editing** | GNU Readline | Custom Raw Termios Mode (`termios.h`) |
| **Codebase Size** | ~180,000 lines of C | ~3,500 lines of focused C11 |
| **Startup Latency** | ~5 to 15 ms (loading dotfiles, locale, readline) | **< 1 ms** (immediate entry to REPL) |
| **Job Control** | Full POSIX job control (`setpgid`, `tcsetpgrp`) | Full POSIX job control (`setpgid`, `tcsetpgrp`) |
| **Process Group Isolation** | Yes | Yes |
| **Pipeline Support** | Arbitrary-length | Arbitrary-length ($N$-stage) |
| **Redirection Types** | `<`, `>`, `>>`, `2>`, `2>>`, `2>&1`, `&>`, `<<<`, `<>` | `<`, `>`, `>>`, `2>`, `2>>`, `2>&1` |
| **Command Substitution** | `$(...)` and legacy `` `...` `` | Modern POSIX `$(...)` via subshell pipes |
| **Filename Globbing** | Custom pathname matcher (`glob.c`) | POSIX `glob()` |
| **Built-in Commands** | 60+ built-ins | 14 essential POSIX built-ins + `stats` |
| **Configuration Files** | `/etc/profile`, `~/.bashrc`, `~/.bash_profile` | `~/.shellcraftrc` |
| **Automated Self-Test** | External test suites (dejagnu/scripts) | Integrated internal `--self-test` harness |

---

## 2. In-Depth Architectural Differences

### 2.1 Parser Construction: Hand-Crafted Recursive Descent vs. Bison
- **GNU Bash**: Uses a complex, legacy LALR(1) parser written in Bison (`parse.y`). Due to shell grammar quirks (e.g., alias expansion within the lexer, heredocs, and contextual keywords), Bash's `parse.y` exceeds 6,000 lines and mixes grammar rules directly with execution state and global variables.
- **Shellcraft**: Separates parsing cleanly into two stages:
  1. A DFA tokenizer (`tokenizer.c`) producing discrete `Token` structs.
  2. A recursive-descent parser (`parser.c`) constructing a clean, typed Abstract Syntax Tree (`ast.c`).
- **Advantage**: Shellcraft's AST can be traversed, inspected, printed, or unit-tested independently of execution.

### 2.2 Terminal Interface: Custom Raw Mode vs. GNU Readline
- **GNU Readline (Bash)**: A comprehensive library providing vi/emacs editing modes, keymap configurations, and wide-character support. However, it introduces heavy terminal dependencies, complex signal interactions, and a non-trivial memory footprint.
- **Shellcraft**: Directly manages terminal I/O using POSIX `termios` system calls. It switches to raw mode (`ICANON`, `ECHO` disabled), parses ANSI escape sequences manually, provides cursor navigation (`Ctrl-A`, `Ctrl-E`, Left, Right), handles command history (`Up`/`Down`), and guarantees clean terminal restoration via `atexit()`.
- **Advantage**: No external link dependencies, total control over terminal state, and zero terminal corruption when child processes terminate.

### 2.3 Concurrency & Process Group Discipline
- **GNU Bash**: Includes decades of backwards compatibility fixes for non-POSIX Unix derivatives (IRIX, SunOS, Ultrix), making its process group and job control code dense with conditional macros (`#ifdef`).
- **Shellcraft**: Adheres strictly to POSIX.1-2008 semantics on modern Linux:
  - Atomic `sigprocmask()` blocking of `SIGCHLD` across `fork()`.
  - Dual `setpgid()` in both child and parent to avoid process group creation races.
  - Consistent use of `_exit()` in all forked child processes, ensuring user-space stdio buffers and `atexit()` handlers in the parent process are not disrupted.

### 2.4 Educational & Research Utility
- GNU Bash is an industrial shell designed for complex shell scripts and legacy backwards compatibility. Its sheer size makes understanding core OS concepts challenging.
- **Shellcraft** is engineered as a clean, reference implementation of modern systems programming: every major OS primitive—processes, threads, pipes, signals, file descriptors, terminal arbitration, and ASTs—is cleanly isolated and implemented with textbook rigor.
