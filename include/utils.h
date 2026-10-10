#ifndef SHELLCRAFT_UTILS_H
#define SHELLCRAFT_UTILS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

/* Safe memory management wrappers */
void *safe_malloc(size_t size);
void *safe_calloc(size_t nmemb, size_t size);
void *safe_realloc(void *ptr, size_t size);
char *safe_strdup(const char *s);

/* String utilities */
char *str_trim(char *str);
bool str_has_prefix(const char *str, const char *prefix);
bool str_has_suffix(const char *str, const char *suffix);
char *str_replace(const char *orig, const char *rep, const char *with);
char **str_split_whitespace(const char *str, int *count);
void free_str_array(char **arr, int count);

/* Time utilities (for command execution measurement) */
long get_current_time_ms(void);

/* Error and diagnostic reporting */
void shell_error(const char *fmt, ...);
void shell_warn(const char *fmt, ...);
void shell_info(const char *fmt, ...);
void debug_print(const char *fmt, ...);

/* Debug state control */
void set_debug_mode(bool enable);
bool is_debug_mode(void);

#endif /* SHELLCRAFT_UTILS_H */
