#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>

static bool g_debug_mode = false;

void *safe_malloc(size_t size) {
    if (size == 0) return NULL;
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "shellcraft: fatal error: out of memory (malloc %zu bytes)\n", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *safe_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;
    void *ptr = calloc(nmemb, size);
    if (!ptr) {
        fprintf(stderr, "shellcraft: fatal error: out of memory (calloc %zu x %zu bytes)\n", nmemb, size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *safe_realloc(void *ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        fprintf(stderr, "shellcraft: fatal error: out of memory (realloc %zu bytes)\n", size);
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

char *safe_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = safe_malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

char *str_trim(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return str;
}

bool str_has_prefix(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool str_has_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suf_len = strlen(suffix);
    if (suf_len > str_len) return false;
    return strcmp(str + (str_len - suf_len), suffix) == 0;
}

char *str_replace(const char *orig, const char *rep, const char *with) {
    if (!orig || !rep) return NULL;
    if (!with) with = "";
    size_t rep_len = strlen(rep);
    if (rep_len == 0) return safe_strdup(orig);
    size_t with_len = strlen(with);

    /* Count occurrences */
    int count = 0;
    const char *ins = orig;
    const char *tmp;
    while ((tmp = strstr(ins, rep))) {
        count++;
        ins = tmp + rep_len;
    }

    size_t result_len = strlen(orig) + (with_len - rep_len) * count;
    char *result = safe_malloc(result_len + 1);
    char *dst = result;
    ins = orig;
    while ((tmp = strstr(ins, rep))) {
        size_t len_front = tmp - ins;
        memcpy(dst, ins, len_front);
        dst += len_front;
        memcpy(dst, with, with_len);
        dst += with_len;
        ins = tmp + rep_len;
    }
    strcpy(dst, ins);
    return result;
}

char **str_split_whitespace(const char *str, int *count) {
    if (!str || !count) return NULL;
    int cap = 8;
    int n = 0;
    char **arr = safe_malloc(sizeof(char *) * cap);

    const char *p = str;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t len = p - start;
        char *word = safe_malloc(len + 1);
        memcpy(word, start, len);
        word[len] = '\0';

        if (n >= cap) {
            cap *= 2;
            arr = safe_realloc(arr, sizeof(char *) * cap);
        }
        arr[n++] = word;
    }

    *count = n;
    return arr;
}

void free_str_array(char **arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++) {
        if (arr[i]) free(arr[i]);
    }
    free(arr);
}

long get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void shell_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "shellcraft: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void shell_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "shellcraft: warning: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void shell_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void debug_print(const char *fmt, ...) {
    if (!g_debug_mode) return;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[DEBUG] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void set_debug_mode(bool enable) {
    g_debug_mode = enable;
}

bool is_debug_mode(void) {
    return g_debug_mode;
}
