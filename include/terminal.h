#ifndef SHELLCRAFT_TERMINAL_H
#define SHELLCRAFT_TERMINAL_H

#include <stdbool.h>
#include <termios.h>

/* Terminal mode control */
void terminal_init(void);
void terminal_restore(void);
bool terminal_is_interactive(void);
void terminal_save_state(struct termios *tmodes);
void terminal_set_state(const struct termios *tmodes);

/* Raw mode interactive line reader with line editing and history navigation */
char *terminal_readline(const char *prompt);

#endif /* SHELLCRAFT_TERMINAL_H */
