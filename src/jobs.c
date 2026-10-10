#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "jobs.h"
#include "signals.h"
#include "terminal.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

static Job *g_job_list = NULL;
static int g_next_job_id = 1;
static int g_completed_count = 0;

void jobs_init(void) {
    g_job_list = NULL;
    g_next_job_id = 1;
    g_completed_count = 0;
}

void jobs_cleanup(void) {
    Job *curr = g_job_list;
    while (curr) {
        Job *next = curr->next;
        if (curr->command_line) free(curr->command_line);
        if (curr->processes) free(curr->processes);
        free(curr);
        curr = next;
    }
    g_job_list = NULL;
}

Job *job_create(pid_t pgid, const char *cmd, int num_procs, const pid_t *pids, JobState initial_state) {
    sigset_t oldmask;
    block_sigchld(&oldmask);

    Job *job = safe_malloc(sizeof(Job));
    job->id = g_next_job_id++;
    job->pgid = pgid;
    job->command_line = safe_strdup(cmd ? cmd : "");
    job->process_count = num_procs;
    job->processes = safe_malloc(sizeof(Process) * num_procs);
    job->state = initial_state;
    job->notified = false;
    job->next = NULL;
    memset(&job->tmodes, 0, sizeof(struct termios));

    for (int i = 0; i < num_procs; i++) {
        job->processes[i].pid = pids[i];
        job->processes[i].completed = false;
        job->processes[i].stopped = false;
        job->processes[i].status = 0;
    }

    if (!g_job_list) {
        g_job_list = job;
    } else {
        Job *curr = g_job_list;
        while (curr->next) curr = curr->next;
        curr->next = job;
    }

    unblock_sigchld(&oldmask);
    debug_print("Created Job [%d] pgid=%d, procs=%d, cmd='%s'", job->id, job->pgid, num_procs, job->command_line);
    return job;
}

Job *job_find_by_id(int id) {
    Job *curr = g_job_list;
    while (curr) {
        if (curr->id == id) return curr;
        curr = curr->next;
    }
    return NULL;
}

Job *job_find_by_pgid(pid_t pgid) {
    Job *curr = g_job_list;
    while (curr) {
        if (curr->pgid == pgid) return curr;
        curr = curr->next;
    }
    return NULL;
}

Job *job_find_by_pid(pid_t pid) {
    Job *curr = g_job_list;
    while (curr) {
        for (int i = 0; i < curr->process_count; i++) {
            if (curr->processes[i].pid == pid) return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

void job_remove(Job *job) {
    if (!job) return;
    sigset_t oldmask;
    block_sigchld(&oldmask);

    if (g_job_list == job) {
        g_job_list = job->next;
    } else {
        Job *curr = g_job_list;
        while (curr && curr->next != job) curr = curr->next;
        if (curr) curr->next = job->next;
    }

    if (job->command_line) free(job->command_line);
    if (job->processes) free(job->processes);
    free(job);

    unblock_sigchld(&oldmask);
}

static bool job_is_completed(const Job *job) {
    for (int i = 0; i < job->process_count; i++) {
        if (!job->processes[i].completed) return false;
    }
    return true;
}

static bool job_is_stopped(const Job *job) {
    if (job_is_completed(job)) return false;
    for (int i = 0; i < job->process_count; i++) {
        if (job->processes[i].stopped) return true;
    }
    return false;
}

void job_update_status(void) {
    int status;
    pid_t pid;

    /* Non-blocking wait for any child process state changes */
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        debug_print("waitpid reap: pid=%d, status=0x%x", pid, status);
        Job *job = job_find_by_pid(pid);
        if (!job) continue;

        for (int i = 0; i < job->process_count; i++) {
            if (job->processes[i].pid == pid) {
                job->processes[i].status = status;
                if (WIFSTOPPED(status)) {
                    job->processes[i].stopped = true;
                } else if (WIFCONTINUED(status)) {
                    job->processes[i].stopped = false;
                } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    job->processes[i].completed = true;
                    job->processes[i].stopped = false;
                }
                break;
            }
        }

        if (job_is_completed(job)) {
            bool signaled = false;
            for (int i = 0; i < job->process_count; i++) {
                if (WIFSIGNALED(job->processes[i].status)) {
                    signaled = true;
                    break;
                }
            }
            job->state = signaled ? JOB_TERMINATED : JOB_DONE;
            g_completed_count++;
        } else if (job_is_stopped(job)) {
            job->state = JOB_STOPPED;
        } else {
            job->state = JOB_RUNNING;
        }
    }
}

void job_notify_completed(void) {
    job_update_status();

    sigset_t oldmask;
    block_sigchld(&oldmask);

    Job *curr = g_job_list;
    Job *prev = NULL;

    while (curr) {
        Job *next = curr->next;
        if (curr->state == JOB_DONE || curr->state == JOB_TERMINATED) {
            if (!curr->notified) {
                printf("[%d]+  %-12s %s\n",
                       curr->id,
                       job_state_name(curr->state),
                       curr->command_line);
                curr->notified = true;
            }
            /* Remove completed job */
            if (prev) {
                prev->next = next;
            } else {
                g_job_list = next;
            }
            if (curr->command_line) free(curr->command_line);
            if (curr->processes) free(curr->processes);
            free(curr);
        } else {
            prev = curr;
        }
        curr = next;
    }

    unblock_sigchld(&oldmask);
}

int job_wait_foreground(Job *job, int *last_status) {
    if (!job) return -1;
    pid_t shell_pgid = getpgrp();

    bool controls_terminal = terminal_is_interactive() && (tcgetpgrp(STDIN_FILENO) == shell_pgid);

    /* Put the job into the foreground of the controlling terminal */
    if (controls_terminal && job->pgid > 0 && job->pgid != shell_pgid) {
        tcsetpgrp(STDIN_FILENO, job->pgid);
    }

    int status = 0;
    while (!job_is_completed(job) && !job_is_stopped(job)) {
        pid_t wait_target = -1;
        if (controls_terminal && job->pgid > 0 && job->pgid != shell_pgid) {
            wait_target = -job->pgid;
        } else {
            for (int i = 0; i < job->process_count; i++) {
                if (!job->processes[i].completed && !job->processes[i].stopped) {
                    wait_target = job->processes[i].pid;
                    break;
                }
            }
        }
        if (wait_target <= 0) break;

        pid_t pid = waitpid(wait_target, &status, WUNTRACED);
        if (pid < 0) {
            if (errno == ECHILD) break;
            if (errno == EINTR) continue;
            perror("waitpid");
            break;
        }

        for (int i = 0; i < job->process_count; i++) {
            if (job->processes[i].pid == pid) {
                job->processes[i].status = status;
                if (WIFSTOPPED(status)) {
                    job->processes[i].stopped = true;
                    debug_print("process %d stopped by signal %d (%s)",
                                pid, WSTOPSIG(status), strsignal(WSTOPSIG(status)));
                } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    job->processes[i].completed = true;
                    job->processes[i].stopped = false;
                }
                break;
            }
        }
    }

    /* Restore terminal control to shell */
    if (controls_terminal) {
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    }

    if (job_is_completed(job)) {
        bool signaled = false;
        for (int i = 0; i < job->process_count; i++) {
            if (WIFSIGNALED(job->processes[i].status)) {
                signaled = true;
                break;
            }
        }
        job->state = signaled ? JOB_TERMINATED : JOB_DONE;
        g_completed_count++;
    } else if (job_is_stopped(job)) {
        job->state = JOB_STOPPED;
        printf("\n[%d]+  Stopped                 %s\n", job->id, job->command_line);
    }

    /* Extract exit status of the last command in pipeline */
    if (job->process_count > 0) {
        int final_raw = job->processes[job->process_count - 1].status;
        if (WIFEXITED(final_raw)) {
            status = WEXITSTATUS(final_raw);
        } else if (WIFSIGNALED(final_raw)) {
            status = 128 + WTERMSIG(final_raw);
        }
    }
    if (last_status) *last_status = status;

    /* If job finished, remove it immediately */
    if (job->state == JOB_DONE || job->state == JOB_TERMINATED) {
        job_remove(job);
    }

    return status;
}

int job_put_in_foreground(Job *job, bool cont) {
    if (!job) return -1;
    printf("%s\n", job->command_line);
    if (cont) {
        job->state = JOB_RUNNING;
        for (int i = 0; i < job->process_count; i++) {
            job->processes[i].stopped = false;
        }
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
        }
    }
    int status = 0;
    return job_wait_foreground(job, &status);
}

int job_put_in_background(Job *job, bool cont) {
    if (!job) return -1;
    if (cont) {
        job->state = JOB_RUNNING;
        for (int i = 0; i < job->process_count; i++) {
            job->processes[i].stopped = false;
        }
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
        }
    }
    printf("[%d] %d\n", job->id, job->pgid);
    return 0;
}

const char *job_state_name(JobState state) {
    switch (state) {
        case JOB_RUNNING: return "Running";
        case JOB_STOPPED: return "Stopped";
        case JOB_DONE: return "Done";
        case JOB_TERMINATED: return "Terminated";
        default: return "Unknown";
    }
}

void job_print_all(bool only_active) {
    job_update_status();
    Job *curr = g_job_list;
    while (curr) {
        if (!only_active || curr->state == JOB_RUNNING || curr->state == JOB_STOPPED) {
            printf("[%d]+  %-12s %s%s\n",
                   curr->id,
                   job_state_name(curr->state),
                   curr->command_line,
                   curr->state == JOB_RUNNING ? " &" : "");
        }
        curr = curr->next;
    }
}

int job_count_active(void) {
    job_update_status();
    int count = 0;
    Job *curr = g_job_list;
    while (curr) {
        if (curr->state == JOB_RUNNING || curr->state == JOB_STOPPED) {
            count++;
        }
        curr = curr->next;
    }
    return count;
}

int job_count_completed(void) {
    return g_completed_count;
}

int job_reap_zombies(void) {
    int status;
    int reaped = 0;
    while (waitpid(-1, &status, WNOHANG) > 0) {
        reaped++;
    }
    return reaped;
}
