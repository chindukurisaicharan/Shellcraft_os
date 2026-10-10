#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "terminal.h"
#include "history.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>

static struct termios g_orig_termios;
static bool g_terminal_initialized = false;
static bool g_raw_mode_active = false;

bool terminal_is_interactive(void) {
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}

void terminal_restore(void) {
    if (g_raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_mode_active = false;
    }
}

static void enable_raw_mode(void) {
    if (!terminal_is_interactive() || g_raw_mode_active) return;
    struct termios raw = g_orig_termios;
    /* Disable canonical mode, echo, extended input processing, and flow control */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    /* Allow INT and TSTP or keep signals managed */
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    g_raw_mode_active = true;
}

static void disable_raw_mode(void) {
    terminal_restore();
}

void terminal_init(void) {
    if (g_terminal_initialized) return;
    if (terminal_is_interactive()) {
        if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
            g_terminal_initialized = true;
            atexit(terminal_restore);
        }
    }
}

void terminal_save_state(struct termios *tmodes) {
    if (tmodes && terminal_is_interactive()) {
        tcgetattr(STDIN_FILENO, tmodes);
    }
}

void terminal_set_state(const struct termios *tmodes) {
    if (tmodes && terminal_is_interactive()) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, tmodes);
    }
}

static void refresh_line(const char *prompt, const char *buf, size_t pos) {
    /* Write \r, prompt, buffer, clear to end of line (\033[K), move cursor back */
    size_t prompt_len = strlen(prompt);
    printf("\r%s%s\033[K", prompt, buf);
    /* Position cursor */
    size_t total_cursor = prompt_len + pos;
    printf("\r\033[%zuC", total_cursor);
    fflush(stdout);
}

char *terminal_readline(const char *prompt) {
    if (!prompt) prompt = "";

    /* If not running in an interactive terminal, use standard getline */
    if (!terminal_is_interactive()) {
        char *line = NULL;
        size_t len = 0;
        ssize_t read = getline(&line, &len, stdin);
        if (read == -1) {
            free(line);
            return NULL;
        }
        while (read > 0 && (line[read - 1] == '\n' || line[read - 1] == '\r')) {
            line[read - 1] = '\0';
            read--;
        }
        return line;
    }

    enable_raw_mode();
    printf("%s", prompt);
    fflush(stdout);

    size_t cap = 256;
    char *buf = safe_malloc(cap);
    size_t len = 0;
    size_t pos = 0;
    buf[0] = '\0';

    int hist_index = history_count();
    char *saved_draft = NULL;

    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }

        /* Ctrl-D: EOF */
        if (c == 4) {
            if (len == 0) {
                free(buf);
                if (saved_draft) free(saved_draft);
                disable_raw_mode();
                return NULL;
            }
            continue;
        }

        /* Ctrl-C: Cancel line */
        if (c == 3) {
            printf("^C\n");
            fflush(stdout);
            buf[0] = '\0';
            len = 0;
            pos = 0;
            if (saved_draft) { free(saved_draft); saved_draft = NULL; }
            break;
        }

        /* Enter: Submit line */
        if (c == '\r' || c == '\n') {
            printf("\n");
            fflush(stdout);
            break;
        }

        /* Backspace (127 or 8) */
        if (c == 127 || c == 8) {
            if (pos > 0) {
                memmove(&buf[pos - 1], &buf[pos], len - pos + 1);
                pos--;
                len--;
                refresh_line(prompt, buf, pos);
            }
            continue;
        }

        /* Ctrl-A: Beginning of line */
        if (c == 1) {
            pos = 0;
            refresh_line(prompt, buf, pos);
            continue;
        }

        /* Ctrl-E: End of line */
        if (c == 5) {
            pos = len;
            refresh_line(prompt, buf, pos);
            continue;
        }

        /* Ctrl-K: Kill text to end of line */
        if (c == 11) {
            buf[pos] = '\0';
            len = pos;
            refresh_line(prompt, buf, pos);
            continue;
        }

        /* Ctrl-U: Kill entire line */
        if (c == 21) {
            buf[0] = '\0';
            pos = 0;
            len = 0;
            refresh_line(prompt, buf, pos);
            continue;
        }

        /* Ctrl-L: Clear screen */
        if (c == 12) {
            printf("\033[2J\033[H");
            refresh_line(prompt, buf, pos);
            continue;
        }

        /* Escape sequence (arrows, delete, etc.) */
        if (c == 27) {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;

            if (seq[0] == '[') {
                if (seq[1] == 'A') {
                    /* Up arrow: history previous */
                    if (hist_index > 0) {
                        if (hist_index == history_count()) {
                            if (saved_draft) free(saved_draft);
                            saved_draft = safe_strdup(buf);
                        }
                        hist_index--;
                        const char *h = history_get(hist_index);
                        if (h) {
                            size_t hlen = strlen(h);
                            if (hlen + 1 >= cap) {
                                cap = hlen + 64;
                                buf = safe_realloc(buf, cap);
                            }
                            strcpy(buf, h);
                            len = hlen;
                            pos = len;
                            refresh_line(prompt, buf, pos);
                        }
                    }
                } else if (seq[1] == 'B') {
                    /* Down arrow: history next */
                    if (hist_index < history_count()) {
                        hist_index++;
                        const char *h = (hist_index == history_count()) ? (saved_draft ? saved_draft : "") : history_get(hist_index);
                        if (h) {
                            size_t hlen = strlen(h);
                            if (hlen + 1 >= cap) {
                                cap = hlen + 64;
                                buf = safe_realloc(buf, cap);
                            }
                            strcpy(buf, h);
                            len = hlen;
                            pos = len;
                            refresh_line(prompt, buf, pos);
                        }
                    }
                } else if (seq[1] == 'C') {
                    /* Right arrow */
                    if (pos < len) {
                        pos++;
                        refresh_line(prompt, buf, pos);
                    }
                } else if (seq[1] == 'D') {
                    /* Left arrow */
                    if (pos > 0) {
                        pos--;
                        refresh_line(prompt, buf, pos);
                    }
                } else if (seq[1] == 'H') {
                    /* Home */
                    pos = 0;
                    refresh_line(prompt, buf, pos);
                } else if (seq[1] == 'F') {
                    /* End */
                    pos = len;
                    refresh_line(prompt, buf, pos);
                } else if (seq[1] == '3') {
                    /* Delete key: reads '~' */
                    char t;
                    if (read(STDIN_FILENO, &t, 1) > 0 && t == '~') {
                        if (pos < len) {
                            memmove(&buf[pos], &buf[pos + 1], len - pos);
                            len--;
                            refresh_line(prompt, buf, pos);
                        }
                    }
                }
            }
            continue;
        }

        /* Printable character */
        if (!iscntrl((unsigned char)c)) {
            if (len + 2 >= cap) {
                cap *= 2;
                buf = safe_realloc(buf, cap);
            }
            memmove(&buf[pos + 1], &buf[pos], len - pos + 1);
            buf[pos] = c;
            pos++;
            len++;
            refresh_line(prompt, buf, pos);
        }
    }

    if (saved_draft) free(saved_draft);
    disable_raw_mode();
    return buf;
}
