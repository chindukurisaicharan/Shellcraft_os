# Shellcraft Live Evaluation & Demonstration Guide

This guide is designed for project evaluators, professors, and examiners conducting a live demonstration or viva voce examination of **Shellcraft**.

---

## 1. Quick Demonstration Script

An automated demonstration script exercising 10 distinct OS scenarios with formatted commentary is provided:

```bash
bash scripts/demo.sh
```

To run individual commands interactively inside Shellcraft:
```bash
./shellcraft
```

---

## 2. 10 Core Demonstration Scenarios

### Scenario 1: Built-in Commands & Environment Persistence
Demonstrates that built-in commands run inside the shell's process address space without invoking `fork()`.
```shell
pwd
cd /tmp
pwd
cd -
export COURSE="Operating Systems"
echo "Enrolled in: $COURSE"
```
- **OS Concept**: If `cd` or `export` were executed in a child process, the directory change and environment variable would be discarded upon child termination. Shellcraft executes built-ins directly in the parent process.

---

### Scenario 2: Multi-Stage Pipelines ($N$-Stage)
Demonstrates arbitrary pipelines connected via unidirectional kernel pipes.
```shell
cat /etc/passwd | cut -d: -f1 | sort | head -n 5
```
- **OS Concept**: Shellcraft creates 3 pipes (`pipe()`), forks 4 child processes, wires standard I/O via `dup2()`, and closes all 6 pipe file descriptor ends in both children and parent. Concurrency is verified as all 4 processes run simultaneously in the kernel.

---

### Scenario 3: Complete Redirection Matrix
Demonstrates file descriptor redirection and cloning.
```shell
echo "First line" > /tmp/shellcraft_test.txt
echo "Second line" >> /tmp/shellcraft_test.txt
cat < /tmp/shellcraft_test.txt
ls /nonexistent_directory 2> /tmp/shellcraft_err.txt
cat /tmp/shellcraft_err.txt
ls /nonexistent_directory /tmp 2>&1 | head -n 3
```
- **OS Concept**:
  - `>` uses `open(..., O_WRONLY | O_CREAT | O_TRUNC, 0644)` + `dup2(fd, STDOUT_FILENO)`.
  - `>>` uses `open(..., O_WRONLY | O_CREAT | O_APPEND, 0644)`.
  - `<` uses `open(..., O_RDONLY)` + `dup2(fd, STDIN_FILENO)`.
  - `2>&1` duplicates `STDOUT_FILENO` into `STDERR_FILENO`.

---

### Scenario 4: Dynamic Expansions & Command Substitutions
Demonstrates macro expansion, environment substitution, tilde path resolution, and nested subshells.
```shell
echo "User: $USER | Shell PID: $$"
echo "Home directory: ~"
echo "Subshell output: Today is $(date +%A)"
echo "Pipeline substitution: $(echo 'alpha beta gamma' | tr ' ' '\n' | sort | head -n 1)"
```
- **OS Concept**: `$(cmd)` forks an isolated child subshell, binds its standard output to a pipe, reads the captured output into the parent, trims trailing newlines, and substitutes the text in place.

---

### Scenario 5: Conditional Logic & Short-Circuiting
Demonstrates boolean evaluation of process return codes (`$?`).
```shell
true && echo "Condition 1 Passed"
false && echo "This will NOT print"
false || echo "Fallback Executed"
true || echo "This will NOT print"
```
- **OS Concept**: Conforms to standard shell short-circuiting: `cmd1 && cmd2` executes `cmd2` only if `cmd1` returns exit status 0 (`WEXITSTATUS(status) == 0`). `cmd1 || cmd2` executes `cmd2` only if `cmd1` returns non-zero.

---

### Scenario 6: Background Processes & Non-Blocking REPL
Demonstrates process group detachment and prompt return latency.
```shell
sleep 5 &
jobs
```
- **OS Concept**: The child process is placed in a new process group (`setpgid(pid, pid)`). The shell does not hand off terminal ownership (`tcsetpgrp()`) and does not block on `waitpid()`. The interactive prompt returns within **< 15 ms**.

---

### Scenario 7: Interactive Job Control (`jobs`, `fg`, `bg`)
Demonstrates terminal arbitration and job state transitions.
1. Run a long-running process in foreground:
   ```shell
   sleep 30
   ```
2. Suspend it with `Ctrl-Z`:
   ```shell
   ^Z
   [1]+ Stopped    sleep 30
   ```
3. Inspect job table:
   ```shell
   jobs
   ```
4. Resume in background:
   ```shell
   bg %1
   [1]+ sleep 30 &
   ```
5. Bring back to foreground:
   ```shell
   fg %1
   ```
- **OS Concept**:
  - `Ctrl-Z` triggers `SIGTSTP`.
  - `bg` issues `kill(-pgid, SIGCONT)` without transferring terminal ownership.
  - `fg` issues `tcsetpgrp(STDIN_FILENO, pgid)` followed by `kill(-pgid, SIGCONT)` and blocks using `waitpid()`.

---

### Scenario 8: Signal Immunity & Forwarding
Demonstrates signal mask isolation.
1. Run `sleep 20` in foreground. Press `Ctrl-C`.
   - The child process terminates with signal 2 (`SIGINT`).
   - The shell catches the notification, prints a newline, updates `$? = 130` (128 + 2), and prompts for next input without crashing.
2. At the empty shell prompt, press `Ctrl-C`.
   - The shell catches `SIGINT`, cancels any partially typed line, and renders a fresh prompt.

---

### Scenario 9: Live Shell Telemetry (`stats`)
Demonstrates session-wide metrics tracking:
```shell
stats
```
Example Output:
```
Shellcraft Runtime Telemetry:
  Commands executed  : 24
  Pipelines executed : 5
  Jobs launched      : 3
  Active background  : 0
```

---

### Scenario 10: Subsystem Self-Test (`--self-test`)
Demonstrates automated self-verification of all 11 core operating system mechanisms:
```shell
./shellcraft --self-test
```
Verifies:
1. Lexical Tokenizer
2. Recursive-Descent Parser
3. Process Fork/Exec
4. Arbitrary Pipeline IPC
5. File Descriptor Redirection
6. Environment & Variable Expansion
7. Built-in Command Dispatch
8. Job Control & Process Groups
9. Command History Persistence
10. Command Substitution Subshells
11. Asynchronous Child Reaping (0 Zombies)
