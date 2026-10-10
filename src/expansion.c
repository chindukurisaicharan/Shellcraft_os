#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "expansion.h"
#include "shell.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <glob.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

char *expand_variables_and_tilde(const char *input, int last_exit_status, pid_t shell_pid) {
    if (!input) return NULL;

    size_t cap = 128;
    char *out = safe_malloc(cap);
    size_t out_len = 0;

    const char *p = input;

    /* Tilde expansion at start of token */
    if (*p == '~') {
        if (*(p + 1) == '\0' || *(p + 1) == '/') {
            const char *home = getenv("HOME");
            if (!home) {
                struct passwd *pw = getpwuid(getuid());
                if (pw) home = pw->pw_dir;
            }
            if (home) {
                size_t hlen = strlen(home);
                if (out_len + hlen + 1 >= cap) {
                    cap = out_len + hlen + 64;
                    out = safe_realloc(out, cap);
                }
                memcpy(out + out_len, home, hlen);
                out_len += hlen;
                p++; /* Skip ~ */
            }
        }
    }

    while (*p) {
        if (*p == '$') {
            p++;
            if (*p == '?') {
                /* $? - Last exit status */
                char num[32];
                snprintf(num, sizeof(num), "%d", last_exit_status);
                size_t nlen = strlen(num);
                if (out_len + nlen + 1 >= cap) {
                    cap = out_len + nlen + 64;
                    out = safe_realloc(out, cap);
                }
                memcpy(out + out_len, num, nlen);
                out_len += nlen;
                p++;
            } else if (*p == '$') {
                /* $$ - Shell PID */
                char num[32];
                snprintf(num, sizeof(num), "%d", (int)shell_pid);
                size_t nlen = strlen(num);
                if (out_len + nlen + 1 >= cap) {
                    cap = out_len + nlen + 64;
                    out = safe_realloc(out, cap);
                }
                memcpy(out + out_len, num, nlen);
                out_len += nlen;
                p++;
            } else if (*p == '{') {
                /* ${VAR} */
                p++;
                const char *start = p;
                while (*p && *p != '}') p++;
                if (*p == '}') {
                    size_t var_len = p - start;
                    char var_name[256];
                    if (var_len < sizeof(var_name)) {
                        memcpy(var_name, start, var_len);
                        var_name[var_len] = '\0';
                        const char *val = getenv(var_name);
                        if (val) {
                            size_t vlen = strlen(val);
                            if (out_len + vlen + 1 >= cap) {
                                cap = out_len + vlen + 64;
                                out = safe_realloc(out, cap);
                            }
                            memcpy(out + out_len, val, vlen);
                            out_len += vlen;
                        }
                    }
                    p++; /* Skip } */
                }
            } else if (isalpha((unsigned char)*p) || *p == '_') {
                /* $VAR */
                const char *start = p;
                while (isalnum((unsigned char)*p) || *p == '_') p++;
                size_t var_len = p - start;
                char var_name[256];
                if (var_len < sizeof(var_name)) {
                    memcpy(var_name, start, var_len);
                    var_name[var_len] = '\0';
                    const char *val = getenv(var_name);
                    if (val) {
                        size_t vlen = strlen(val);
                        if (out_len + vlen + 1 >= cap) {
                            cap = out_len + vlen + 64;
                            out = safe_realloc(out, cap);
                        }
                        memcpy(out + out_len, val, vlen);
                        out_len += vlen;
                    }
                }
            } else {
                /* Literal $ */
                if (out_len + 2 >= cap) {
                    cap *= 2;
                    out = safe_realloc(out, cap);
                }
                out[out_len++] = '$';
            }
        } else {
            if (out_len + 2 >= cap) {
                cap *= 2;
                out = safe_realloc(out, cap);
            }
            out[out_len++] = *p++;
        }
    }

    out[out_len] = '\0';
    return out;
}

char *expand_command_substitutions(const char *input, ShellState *shell) {
    if (!input || !strstr(input, "$(")) return safe_strdup(input);

    size_t cap = 256;
    char *out = safe_malloc(cap);
    size_t out_len = 0;

    const char *p = input;
    while (*p) {
        if (p[0] == '$' && p[1] == '(') {
            p += 2;
            const char *start = p;
            int depth = 1;
            while (*p && depth > 0) {
                if (*p == '(') depth++;
                else if (*p == ')') depth--;
                if (depth > 0) p++;
            }

            size_t subcmd_len = p - start;
            char *subcmd = safe_malloc(subcmd_len + 1);
            memcpy(subcmd, start, subcmd_len);
            subcmd[subcmd_len] = '\0';
            if (*p == ')') p++;

            debug_print("Command substitution executing: '%s'", subcmd);

            /* Run nested command through Shellcraft's execution engine via pipe */
            int pipefd[2];
            if (pipe(pipefd) < 0) {
                perror("pipe for command substitution");
                free(subcmd);
                continue;
            }

            pid_t pid = fork();
            if (pid == 0) {
                /* Child: redirect stdout to pipe */
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);

                /* Disconnect child from parent script stdin */
                int devnull = open("/dev/null", O_RDONLY);
                if (devnull >= 0) {
                    dup2(devnull, STDIN_FILENO);
                    close(devnull);
                }

                /* Non-interactive subshell */
                shell->is_interactive = false;
                int ret = shell_execute_line(shell, subcmd);
                _exit(ret);
            } else if (pid > 0) {
                /* Parent: read stdout */
                close(pipefd[1]);
                size_t res_cap = 256;
                char *res_buf = safe_malloc(res_cap);
                size_t res_len = 0;
                char read_chunk[128];
                ssize_t bytes;

                while ((bytes = read(pipefd[0], read_chunk, sizeof(read_chunk))) > 0) {
                    if (res_len + bytes + 1 >= res_cap) {
                        res_cap += bytes + 256;
                        res_buf = safe_realloc(res_buf, res_cap);
                    }
                    memcpy(res_buf + res_len, read_chunk, bytes);
                    res_len += bytes;
                }
                close(pipefd[0]);
                waitpid(pid, NULL, 0);

                /* Strip trailing newlines */
                while (res_len > 0 && (res_buf[res_len - 1] == '\n' || res_buf[res_len - 1] == '\r')) {
                    res_len--;
                }
                res_buf[res_len] = '\0';

                /* Append captured output */
                if (out_len + res_len + 1 >= cap) {
                    cap = out_len + res_len + 128;
                    out = safe_realloc(out, cap);
                }
                memcpy(out + out_len, res_buf, res_len);
                out_len += res_len;

                free(res_buf);
                free(subcmd);
            } else {
                perror("fork for command substitution");
                close(pipefd[0]);
                close(pipefd[1]);
                free(subcmd);
            }
        } else {
            if (out_len + 2 >= cap) {
                cap *= 2;
                out = safe_realloc(out, cap);
            }
            out[out_len++] = *p++;
        }
    }

    out[out_len] = '\0';
    return out;
}

static bool contains_glob_chars(const char *str) {
    if (!str) return false;
    return strchr(str, '*') || strchr(str, '?') || strchr(str, '[');
}

char **expand_glob_tokens(char **raw_argv, int raw_argc, int *new_argc) {
    if (!raw_argv || raw_argc <= 0) {
        *new_argc = 0;
        return NULL;
    }

    int cap = raw_argc + 8;
    char **expanded = safe_malloc(sizeof(char *) * cap);
    int count = 0;

    for (int i = 0; i < raw_argc; i++) {
        const char *arg = raw_argv[i];
        if (contains_glob_chars(arg)) {
            glob_t globbuf;
            int ret = glob(arg, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf);
            if (ret == 0 && globbuf.gl_pathc > 0) {
                for (size_t g = 0; g < globbuf.gl_pathc; g++) {
                    if (count + 1 >= cap) {
                        cap *= 2;
                        expanded = safe_realloc(expanded, sizeof(char *) * cap);
                    }
                    expanded[count++] = safe_strdup(globbuf.gl_pathv[g]);
                }
                globfree(&globbuf);
                continue;
            }
            globfree(&globbuf);
        }

        if (count + 1 >= cap) {
            cap *= 2;
            expanded = safe_realloc(expanded, sizeof(char *) * cap);
        }
        expanded[count++] = safe_strdup(arg);
    }

    expanded[count] = NULL;
    *new_argc = count;
    return expanded;
}

char **expand_command_args(char **raw_argv, int raw_argc, int *new_argc, ShellState *shell) {
    if (!raw_argv || raw_argc <= 0) {
        *new_argc = 0;
        return NULL;
    }

    char **stage1 = safe_malloc(sizeof(char *) * (raw_argc + 1));
    for (int i = 0; i < raw_argc; i++) {
        /* 1. Command substitution */
        char *sub_exp = expand_command_substitutions(raw_argv[i], shell);
        /* 2. Variable and tilde expansion */
        char *var_exp = expand_variables_and_tilde(sub_exp, shell ? shell->last_exit_status : 0,
                                                   shell ? shell->shell_pid : getpid());
        free(sub_exp);
        stage1[i] = var_exp;
    }
    stage1[raw_argc] = NULL;

    /* 3. Globbing expansion */
    int glob_count = 0;
    char **final_argv = expand_glob_tokens(stage1, raw_argc, &glob_count);

    for (int i = 0; i < raw_argc; i++) {
        free(stage1[i]);
    }
    free(stage1);

    *new_argc = glob_count;
    return final_argv;
}
