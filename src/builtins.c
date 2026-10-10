#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "builtins.h"
#include "round_robin.h"
#include "shell.h"
#include "jobs.h"
#include "history.h"
#include "utils.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

typedef struct AliasEntry {
    char *name;
    char *value;
} AliasEntry;

static AliasEntry g_aliases[MAX_ALIAS_COUNT];
static int g_alias_count = 0;

void alias_init(void) {
    g_alias_count = 0;
    memset(g_aliases, 0, sizeof(g_aliases));
}

void alias_cleanup(void) {
    for (int i = 0; i < g_alias_count; i++) {
        if (g_aliases[i].name) free(g_aliases[i].name);
        if (g_aliases[i].value) free(g_aliases[i].value);
    }
    g_alias_count = 0;
}

const char *alias_lookup(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_alias_count; i++) {
        if (strcmp(g_aliases[i].name, name) == 0) {
            return g_aliases[i].value;
        }
    }
    return NULL;
}

int builtin_alias(int argc, char **argv) {
    if (argc == 1) {
        for (int i = 0; i < g_alias_count; i++) {
            printf("alias %s='%s'\n", g_aliases[i].name, g_aliases[i].value);
        }
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) {
            const char *val = alias_lookup(argv[i]);
            if (val) {
                printf("alias %s='%s'\n", argv[i], val);
            } else {
                shell_error("alias: %s: not found", argv[i]);
                return 1;
            }
        } else {
            size_t name_len = eq - argv[i];
            char name[128];
            if (name_len >= sizeof(name)) name_len = sizeof(name) - 1;
            memcpy(name, argv[i], name_len);
            name[name_len] = '\0';

            const char *val = eq + 1;
            /* Strip optional surrounding quotes */
            char clean_val[256];
            size_t vlen = strlen(val);
            if (vlen >= 2 && ((val[0] == '\'' && val[vlen - 1] == '\'') || (val[0] == '"' && val[vlen - 1] == '"'))) {
                memcpy(clean_val, val + 1, vlen - 2);
                clean_val[vlen - 2] = '\0';
            } else {
                strncpy(clean_val, val, sizeof(clean_val) - 1);
                clean_val[sizeof(clean_val) - 1] = '\0';
            }

            /* Update if existing */
            bool updated = false;
            for (int a = 0; a < g_alias_count; a++) {
                if (strcmp(g_aliases[a].name, name) == 0) {
                    free(g_aliases[a].value);
                    g_aliases[a].value = safe_strdup(clean_val);
                    updated = true;
                    break;
                }
            }
            if (!updated && g_alias_count < MAX_ALIAS_COUNT) {
                g_aliases[g_alias_count].name = safe_strdup(name);
                g_aliases[g_alias_count].value = safe_strdup(clean_val);
                g_alias_count++;
            }
        }
    }
    return 0;
}

int builtin_unalias(int argc, char **argv) {
    if (argc < 2) {
        shell_error("unalias: usage: unalias <name>");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        bool found = false;
        for (int a = 0; a < g_alias_count; a++) {
            if (strcmp(g_aliases[a].name, argv[i]) == 0) {
                free(g_aliases[a].name);
                free(g_aliases[a].value);
                for (int j = a; j < g_alias_count - 1; j++) {
                    g_aliases[j] = g_aliases[j + 1];
                }
                g_alias_count--;
                found = true;
                break;
            }
        }
        if (!found) {
            shell_error("unalias: %s: not found", argv[i]);
            return 1;
        }
    }
    return 0;
}

bool is_builtin(const char *name) {
    if (!name) return false;
    return strcmp(name, "cd") == 0 ||
           strcmp(name, "exit") == 0 ||
           strcmp(name, "export") == 0 ||
           strcmp(name, "pwd") == 0 ||
           strcmp(name, "echo") == 0 ||
           strcmp(name, "help") == 0 ||
           strcmp(name, "history") == 0 ||
           strcmp(name, "clear") == 0 ||
           strcmp(name, "type") == 0 ||
           strcmp(name, "jobs") == 0 ||
           strcmp(name, "fg") == 0 ||
           strcmp(name, "bg") == 0 ||
           strcmp(name, "stats") == 0 ||
           strcmp(name, "set") == 0 ||
           strcmp(name, "rg") == 0 ||
           strcmp(name, "alias") == 0 ||
           strcmp(name, "unalias") == 0;
}

int builtin_cd(int argc, char **argv, ShellState *shell) {
    (void)shell;
    char current_dir[PATH_MAX];
    if (!getcwd(current_dir, sizeof(current_dir))) {
        current_dir[0] = '\0';
    }

    const char *target = NULL;
    if (argc == 1 || strcmp(argv[1], "~") == 0) {
        target = getenv("HOME");
        if (!target) {
            shell_error("cd: HOME not set");
            return 1;
        }
    } else if (strcmp(argv[1], "-") == 0) {
        target = getenv("OLDPWD");
        if (!target) {
            shell_error("cd: OLDPWD not set");
            return 1;
        }
        printf("%s\n", target);
    } else {
        target = argv[1];
    }

    if (chdir(target) != 0) {
        shell_error("cd: %s: %s", target, strerror(errno));
        return 1;
    }

    char new_dir[PATH_MAX];
    if (getcwd(new_dir, sizeof(new_dir))) {
        if (current_dir[0] != '\0') {
            setenv("OLDPWD", current_dir, 1);
        }
        setenv("PWD", new_dir, 1);
    }

    return 0;
}

int builtin_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
        return 0;
    }
    perror("pwd");
    return 1;
}

int builtin_echo(int argc, char **argv) {
    bool newline = true;
    int start = 1;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = false;
        start = 2;
    }

    for (int i = start; i < argc; i++) {
        printf("%s%s", argv[i], (i == argc - 1) ? "" : " ");
    }
    if (newline) {
        printf("\n");
    }
    fflush(stdout);
    return 0;
}

int builtin_exit(int argc, char **argv, ShellState *shell) {
    int code = shell ? shell->last_exit_status : 0;
    if (argc > 1) {
        code = atoi(argv[1]);
    }
    if (shell) {
        shell->running = false;
        shell->last_exit_status = code;
    }
    return code;
}

int builtin_export(int argc, char **argv) {
    if (argc == 1) {
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("declare -x %s\n", *env);
        }
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            size_t name_len = eq - argv[i];
            char name[256];
            if (name_len >= sizeof(name)) name_len = sizeof(name) - 1;
            memcpy(name, argv[i], name_len);
            name[name_len] = '\0';
            const char *val = eq + 1;
            if (setenv(name, val, 1) != 0) {
                shell_error("export: '%s': %s", name, strerror(errno));
                return 1;
            }
        } else {
            /* If no equals sign, leave existing value or export empty */
            if (!getenv(argv[i])) {
                setenv(argv[i], "", 1);
            }
        }
    }
    return 0;
}

int builtin_help(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\n Shellcraft — The Command Line, Rebuilt (C11)\n");
    printf(" ---------------------------------------------\n");
    printf(" Built-in Commands:\n");
    printf("   cd [dir]              Change current directory (~ and - supported)\n");
    printf("   pwd                   Print current working directory\n");
    printf("   echo [-n] [args]      Print arguments to standard output\n");
    printf("   export [KEY=VAL]      Set and export environment variables\n");
    printf("   jobs                  List background and stopped jobs\n");
    printf("   fg [%%id | pid]        Bring job to foreground\n");
    printf("   bg [%%id | pid]        Resume job in background\n");
    printf("   history [n | clear]   Display or clear command history\n");
    printf("   type <command>        Display information about command type\n");
    printf("   alias [name=val]      Create or display command aliases\n");
    printf("   unalias <name>        Remove a command alias\n");
    printf("   clear                 Clear terminal display\n");
    printf("   stats                 Display process, job, and duration statistics\n");
    printf("   rg [slice] programs   Round-robin schedule programs (default slice 10ms)\n");
    printf("   set [timing|debug]    Toggle shell diagnostic flags (on/off)\n");
    printf("   help                  Show this reference manual\n");
    printf("   exit [status]         Exit shell cleanly\n\n");
    return 0;
}

int builtin_history(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "clear") == 0) {
        history_clear();
        return 0;
    }
    int limit = 0;
    if (argc > 1) {
        limit = atoi(argv[1]);
    }
    history_print(limit);
    return 0;
}

int builtin_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\033[2J\033[H");
    fflush(stdout);
    return 0;
}

int builtin_type(int argc, char **argv) {
    if (argc < 2) {
        shell_error("type: missing operand");
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        const char *cmd = argv[i];
        if (is_builtin(cmd)) {
            printf("%s is a shellcraft built-in\n", cmd);
            continue;
        }
        const char *alias_val = alias_lookup(cmd);
        if (alias_val) {
            printf("%s is an alias for '%s'\n", cmd, alias_val);
            continue;
        }

        /* Check PATH */
        char *path_env = getenv("PATH");
        if (!path_env) path_env = "/bin:/usr/bin";
        char *path_copy = safe_strdup(path_env);
        char *dir = strtok(path_copy, ":");
        bool found = false;

        while (dir) {
            char fullpath[PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, cmd);
            struct stat st;
            if (stat(fullpath, &st) == 0 && (st.st_mode & S_IXUSR)) {
                printf("%s is %s\n", cmd, fullpath);
                found = true;
                break;
            }
            dir = strtok(NULL, ":");
        }
        free(path_copy);

        if (!found) {
            shell_error("type: %s: not found", cmd);
            ret = 1;
        }
    }
    return ret;
}

int builtin_jobs(int argc, char **argv) {
    (void)argc; (void)argv;
    job_print_all(true);
    return 0;
}

int builtin_fg(int argc, char **argv) {
    Job *job = NULL;
    if (argc > 1) {
        const char *arg = argv[1];
        if (arg[0] == '%') arg++;
        int id = atoi(arg);
        job = job_find_by_id(id);
        if (!job) {
            job = job_find_by_pid((pid_t)id);
        }
    } else {
        /* Default to first active or stopped job */
        job_update_status();
        for (int i = 1; i < 1000; i++) {
            Job *j = job_find_by_id(i);
            if (j && (j->state == JOB_RUNNING || j->state == JOB_STOPPED)) {
                job = j;
            }
        }
    }

    if (!job) {
        shell_error("fg: no such job");
        return 1;
    }

    return job_put_in_foreground(job, true);
}

int builtin_bg(int argc, char **argv) {
    Job *job = NULL;
    if (argc > 1) {
        const char *arg = argv[1];
        if (arg[0] == '%') arg++;
        int id = atoi(arg);
        job = job_find_by_id(id);
        if (!job) {
            job = job_find_by_pid((pid_t)id);
        }
    } else {
        job_update_status();
        for (int i = 1; i < 1000; i++) {
            Job *j = job_find_by_id(i);
            if (j && j->state == JOB_STOPPED) {
                job = j;
            }
        }
    }

    if (!job) {
        shell_error("bg: no such job");
        return 1;
    }

    return job_put_in_background(job, true);
}

int builtin_stats(int argc, char **argv, ShellState *shell) {
    (void)argc; (void)argv;
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, "unknown");

    printf("\n Shellcraft Diagnostic Statistics\n");
    printf(" ================================\n");
    printf(" Shell PID         : %d\n", shell ? shell->shell_pid : getpid());
    printf(" Process Group     : %d\n", shell ? shell->shell_pgid : getpgrp());
    printf(" Active Jobs       : %d\n", job_count_active());
    printf(" Completed Jobs    : %d\n", job_count_completed());
    printf(" Last Exit Code    : %d\n", shell ? shell->last_exit_status : 0);
    printf(" Last Duration     : %ld ms\n", shell ? shell->last_command_duration_ms : 0);
    printf(" Working Directory : %s\n\n", cwd);
    return 0;
}

int builtin_set(int argc, char **argv, ShellState *shell) {
    if (argc < 3) {
        printf("Settings:\n");
        printf("  timing : %s\n", (shell && shell->show_timing) ? "on" : "off");
        printf("  debug  : %s\n", is_debug_mode() ? "on" : "off");
        return 0;
    }

    const char *opt = argv[1];
    const char *val = argv[2];
    bool enable = (strcmp(val, "on") == 0 || strcmp(val, "1") == 0 || strcmp(val, "true") == 0);

    if (strcmp(opt, "timing") == 0) {
        if (shell) shell->show_timing = enable;
        printf("timing is %s\n", enable ? "on" : "off");
    } else if (strcmp(opt, "debug") == 0) {
        set_debug_mode(enable);
        if (shell) shell->debug_mode = enable;
        printf("debug mode is %s\n", enable ? "on" : "off");
    } else {
        shell_error("set: unknown option '%s'", opt);
        return 1;
    }
    return 0;
}

int execute_builtin(int argc, char **argv, ShellState *shell) {
    if (argc == 0 || !argv || !argv[0]) return 0;
    const char *cmd = argv[0];

    if (strcmp(cmd, "cd") == 0) return builtin_cd(argc, argv, shell);
    if (strcmp(cmd, "pwd") == 0) return builtin_pwd(argc, argv);
    if (strcmp(cmd, "echo") == 0) return builtin_echo(argc, argv);
    if (strcmp(cmd, "exit") == 0) return builtin_exit(argc, argv, shell);
    if (strcmp(cmd, "export") == 0) return builtin_export(argc, argv);
    if (strcmp(cmd, "jobs") == 0) return builtin_jobs(argc, argv);
    if (strcmp(cmd, "fg") == 0) return builtin_fg(argc, argv);
    if (strcmp(cmd, "bg") == 0) return builtin_bg(argc, argv);
    if (strcmp(cmd, "help") == 0) return builtin_help(argc, argv);
    if (strcmp(cmd, "history") == 0) return builtin_history(argc, argv);
    if (strcmp(cmd, "clear") == 0) return builtin_clear(argc, argv);
    if (strcmp(cmd, "type") == 0) return builtin_type(argc, argv);
    if (strcmp(cmd, "alias") == 0) return builtin_alias(argc, argv);
    if (strcmp(cmd, "unalias") == 0) return builtin_unalias(argc, argv);
    if (strcmp(cmd, "stats") == 0) return builtin_stats(argc, argv, shell);
    if (strcmp(cmd, "set") == 0) return builtin_set(argc, argv, shell);
    if (strcmp(cmd, "rg") == 0) return builtin_rg(argc, argv, shell);

    return -1;
}
