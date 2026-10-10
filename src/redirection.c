#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "redirection.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int apply_redirections(const Redirection *redirs) {
    const Redirection *curr = redirs;

    while (curr) {
        int fd = -1;

        switch (curr->type) {
            case REDIR_INPUT:
                fd = open(curr->target, O_RDONLY);
                if (fd < 0) {
                    shell_error("cannot open '%s': %s", curr->target, strerror(errno));
                    return -1;
                }
                if (dup2(fd, STDIN_FILENO) < 0) {
                    shell_error("dup2 failed for stdin: %s", strerror(errno));
                    close(fd);
                    return -1;
                }
                close(fd);
                debug_print("redirect stdin < %s", curr->target);
                break;

            case REDIR_OUTPUT:
                fd = open(curr->target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    shell_error("cannot open '%s': %s", curr->target, strerror(errno));
                    return -1;
                }
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    shell_error("dup2 failed for stdout: %s", strerror(errno));
                    close(fd);
                    return -1;
                }
                close(fd);
                debug_print("redirect stdout > %s", curr->target);
                break;

            case REDIR_APPEND:
                fd = open(curr->target, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) {
                    shell_error("cannot open '%s': %s", curr->target, strerror(errno));
                    return -1;
                }
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    shell_error("dup2 failed for append: %s", strerror(errno));
                    close(fd);
                    return -1;
                }
                close(fd);
                debug_print("redirect stdout >> %s", curr->target);
                break;

            case REDIR_STDERR:
                fd = open(curr->target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    shell_error("cannot open '%s': %s", curr->target, strerror(errno));
                    return -1;
                }
                if (dup2(fd, STDERR_FILENO) < 0) {
                    shell_error("dup2 failed for stderr: %s", strerror(errno));
                    close(fd);
                    return -1;
                }
                close(fd);
                debug_print("redirect stderr 2> %s", curr->target);
                break;

            case REDIR_STDERR_APPEND:
                fd = open(curr->target, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) {
                    shell_error("cannot open '%s': %s", curr->target, strerror(errno));
                    return -1;
                }
                if (dup2(fd, STDERR_FILENO) < 0) {
                    shell_error("dup2 failed for stderr append: %s", strerror(errno));
                    close(fd);
                    return -1;
                }
                close(fd);
                debug_print("redirect stderr 2>> %s", curr->target);
                break;

            case REDIR_STDERR_TO_STDOUT:
                if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
                    shell_error("dup2 failed for 2>&1: %s", strerror(errno));
                    return -1;
                }
                debug_print("redirect stderr 2>&1");
                break;

            default:
                break;
        }

        curr = curr->next;
    }

    return 0;
}

int save_standard_fds(int saved_fds[3]) {
    saved_fds[0] = dup(STDIN_FILENO);
    saved_fds[1] = dup(STDOUT_FILENO);
    saved_fds[2] = dup(STDERR_FILENO);

    if (saved_fds[0] < 0 || saved_fds[1] < 0 || saved_fds[2] < 0) {
        perror("dup standard fds");
        return -1;
    }
    return 0;
}

void restore_standard_fds(int saved_fds[3]) {
    if (saved_fds[0] >= 0) {
        dup2(saved_fds[0], STDIN_FILENO);
        close(saved_fds[0]);
        saved_fds[0] = -1;
    }
    if (saved_fds[1] >= 0) {
        dup2(saved_fds[1], STDOUT_FILENO);
        close(saved_fds[1]);
        saved_fds[1] = -1;
    }
    if (saved_fds[2] >= 0) {
        dup2(saved_fds[2], STDERR_FILENO);
        close(saved_fds[2]);
        saved_fds[2] = -1;
    }
}
