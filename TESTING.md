# Shellcraft Quality Assurance & Testing Guide

This document details the multi-layer test harness for **Shellcraft**, encompassing unit tests, integration tests, self-tests, concurrency/zombie stress tests, and dynamic memory analysis.

---

## 1. Test Architecture Overview

```
                      ┌──────────────────────────────┐
                      │ Valgrind & AddressSanitizer  │
                      └──────────────┬───────────────┘
                                     │
                      ┌──────────────▼──────────────┐
                      │ Stress & Zombie Tests       │
                      │ (500 commands, 300 jobs)    │
                      └──────────────┬──────────────┘
                                     │
                      ┌──────────────▼──────────────┐
                      │ Subsystem Self-Test (11/11) │
                      └──────────────┬──────────────┘
                                     │
                      ┌──────────────▼──────────────┐
                      │ Integration Tests (Shell)   │
                      │ (Pipes, Jobs, Signals, Subs)│
                      └──────────────┬──────────────┘
                                     │
                      ┌──────────────▼──────────────┐
                      │ Unit Tests (C Assertions)   │
                      │ (Lexer, Parser, AST, Built) │
                      └─────────────────────────────┘
```

---

## 2. Test Execution Quick Reference

```bash
# Run all unit tests and integration tests
make test

# Run internal 11-point subsystem self-test
./shellcraft --self-test
# or
make self-test

# Run high-concurrency 500-command stress test
bash scripts/stress_test.sh

# Run asynchronous child reaping & zombie process prevention test
bash scripts/zombie_test.sh

# Run Valgrind leak and file descriptor hygiene check
bash scripts/valgrind_test.sh

# Run with AddressSanitizer and UndefinedBehaviorSanitizer
make sanitize
```

---

## 3. Unit Test Suites (`tests/`)

All C unit test binaries compile with strict `-std=c11 -Wall -Wextra -Wpedantic` flags and link against the relevant modular object files.

### 3.1 Tokenizer Unit Tests (`tests/test_tokenizer.c`)
- **`test_simple_words`**: Verifies simple whitespace-separated words and token counts.
- **`test_operators`**: Validates pipe (`|`), conditional AND (`&&`), conditional OR (`||`), sequence (`;`), and background (`&`).
- **`test_redirection_tokens`**: Verifies `< `, `>`, `>>`, `2>`, `2>>`, and `2>&1`.
- **`test_quotes_and_escapes`**: Tests single-quote preservation (ignoring special characters inside quotes), double-quote word aggregation, and backslash escape stripping.
- **`test_unmatched_quotes`**: Validates graceful syntax error emission on unclosed quotes without crashes.
- **Result**: `5/5 PASS`

### 3.2 Parser Unit Tests (`tests/test_parser.c`)
- **`test_parse_simple_command`**: Validates AST structure of single command with arguments.
- **`test_parse_multi_stage_pipeline`**: Verifies 3-stage pipeline AST hierarchy and command extraction.
- **`test_parse_conditionals`**: Tests conditional chaining (`cmd1 && cmd2 || cmd3`) and operator precedence.
- **`test_parse_syntax_error`**: Validates error recovery on trailing pipes or misplaced operators.
- **Result**: `4/4 PASS`

### 3.3 Expansion Unit Tests (`tests/test_expansion.c`)
- **`test_env_var_expansion`**: Verifies `$VAR` and `${VAR}` lookup and substitution.
- **`test_special_vars`**: Validates `$?`, `$$`, `$!`, and `$0`.
- **`test_tilde_expansion`**: Validates `~` and `~/dir` expansion to `$HOME`.
- **`test_command_substitution`**: Tests subshell execution `$(echo foo)` and pipeline capture `$(echo a | tr a b)`.
- **Result**: `4/4 PASS`

### 3.4 Built-ins Unit Tests (`tests/test_builtins.c`)
- **`test_builtin_cd_and_pwd`**: Tests directory changing, `cd -`, `cd ~`, and working directory reporting.
- **`test_builtin_export`**: Validates modifying process environment variables.
- **`test_alias_system`**: Tests creating, resolving, listing, and unaliasing command aliases.
- **`test_builtin_type`**: Confirms detection of built-ins, aliases, and external executable binaries.
- **Result**: `4/4 PASS`

### 3.5 Redirection Unit Tests (`tests/test_redirection.c`)
- **`test_stdout_redirection`**: Validates `apply_redirections` with `O_WRONLY | O_CREAT | O_TRUNC` and standard output restoration.
- **`test_stdin_redirection`**: Validates file reading into standard input.
- **Result**: `2/2 PASS`

---

## 4. Integration Test Suites (`tests/*.sh`)

Integration tests run real bash scripts exercising the compiled `./shellcraft` binary end-to-end:

| Test Script | Description | Status |
| :--- | :--- | :--- |
| [`test_pipeline.sh`](file:///d:/New%20folder%20(4)/tests/test_pipeline.sh) | 2-stage, 3-stage, and 4-stage pipeline data flow, sort, and exit status propagation | **PASS** |
| [`test_jobs.sh`](file:///d:/New%20folder%20(4)/tests/test_jobs.sh) | Background execution latency (< 20 ms), concurrent jobs, and `wait` synchronization | **PASS** |
| [`test_signals.sh`](file:///d:/New%20folder%20(4)/tests/test_signals.sh) | SIGINT prompt recovery and external signal termination exit code (128 + signum) | **PASS** |
| [`test_history.sh`](file:///d:/New%20folder%20(4)/tests/test_history.sh) | In-memory history persistence, numbering, and retrieval | **PASS** |
| [`test_command_substitution.sh`](file:///d:/New%20folder%20(4)/tests/test_command_substitution.sh) | Simple and nested `$(...)` execution through subshells | **PASS** |
| [`golden_session.sh`](file:///d:/New%20folder%20(4)/tests/golden_session.sh) | Multi-step interactive script comparing stdout/stderr against exact golden reference | **PASS** |

---

## 5. Benchmarks & System Invariant Verification

### 5.1 Concurrency & Rapid Execution Stress Test (`scripts/stress_test.sh`)
Executes 500 commands rapidly in sequence through Shellcraft's execution engine:
- **Commands Executed**: 500
- **Total Execution Time**: **216 ms**
- **Average Latency**: **~0.43 ms per command**
- **Verification**: `[PASS] Successfully executed 500 commands in 216 ms!`

### 5.2 Zombie Process Prevention Test (`scripts/zombie_test.sh`)
Spawns 300 asynchronous background jobs concurrently (`true &`, `echo 'test' > /dev/null &`, `sleep 0.01 &`):
- **Jobs Launched**: 300
- **Jobs Completed**: 300
- **Zombies Detected in `/proc`**: **0**
- **Verification**: `[PASS] Zero zombie processes detected. Safe asynchronous child reaping verified!`

### 5.3 Memory Leak & File Descriptor Hygiene (`scripts/valgrind_test.sh`)
Runs full Valgrind analysis with `--leak-check=full --show-leak-kinds=all --track-fds=yes`:
```
==386== HEAP SUMMARY:
==386==     in use at exit: 0 bytes in 0 blocks
==386==   total heap usage: 48 allocs, 48 frees, 28,142 bytes allocated
==386== 
==386== All heap blocks were freed -- no leaks are possible
==386== 
==386== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```
- **Definitely lost**: 0 bytes in 0 blocks
- **Indirectly lost**: 0 bytes in 0 blocks
- **Possibly lost**: 0 bytes in 0 blocks
- **Open file descriptors on exit**: STDIN (0), STDOUT (1), STDERR (2) only.
- **Verification**: `[PASS] 0 memory errors detected by Valgrind!`

### 5.4 AddressSanitizer & UndefinedBehaviorSanitizer
Compiling with `-fsanitize=address,undefined -g -O1` passes all 11 subsystem self-tests with zero sanitizer warnings or runtime faults:
- **Result**: `11/11 PASSED` with clean exit status.
