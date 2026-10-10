#ifndef SHELLCRAFT_JOBS_H
#define SHELLCRAFT_JOBS_H

#include <stdbool.h>
#include <sys/types.h>
#include <termios.h>

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE,
    JOB_TERMINATED
} JobState;

typedef struct Process {
    pid_t pid;
    bool completed;
    bool stopped;
    int status;
} Process;

typedef struct Job {
    int id;                         /* 1-based job id */
    pid_t pgid;                     /* Process group id */
    char *command_line;             /* Original command text */
    Process *processes;             /* Array of processes in pipeline */
    int process_count;
    JobState state;
    bool notified;                  /* Whether user was informed of completion */
    struct termios tmodes;          /* Saved terminal attributes */
    struct Job *next;
} Job;

/* Job table lifecycle */
void jobs_init(void);
void jobs_cleanup(void);

/* Job management */
Job *job_create(pid_t pgid, const char *cmd, int num_procs, const pid_t *pids, JobState initial_state);
Job *job_find_by_id(int id);
Job *job_find_by_pgid(pid_t pgid);
Job *job_find_by_pid(pid_t pid);
void job_remove(Job *job);

/* Job state interrogation & synchronization */
void job_update_status(void);
void job_notify_completed(void);
int job_wait_foreground(Job *job, int *last_status);
int job_put_in_foreground(Job *job, bool cont);
int job_put_in_background(Job *job, bool cont);

/* Job display */
void job_print_all(bool only_active);
const char *job_state_name(JobState state);

/* Diagnostics */
int job_count_active(void);
int job_count_completed(void);
int job_reap_zombies(void);

#endif /* SHELLCRAFT_JOBS_H */
