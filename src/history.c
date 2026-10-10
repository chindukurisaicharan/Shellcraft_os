#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "history.h"
#include "config.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct HistoryState {
    char **entries;
    int count;
    int capacity;
} HistoryState;

static HistoryState g_history = {NULL, 0, 0};

void history_init(void) {
    if (g_history.entries) return;
    g_history.capacity = 64;
    g_history.count = 0;
    g_history.entries = safe_malloc(sizeof(char *) * g_history.capacity);
}

void history_cleanup(void) {
    if (!g_history.entries) return;
    for (int i = 0; i < g_history.count; i++) {
        if (g_history.entries[i]) free(g_history.entries[i]);
    }
    free(g_history.entries);
    g_history.entries = NULL;
    g_history.count = 0;
    g_history.capacity = 0;
}

void history_add(const char *line) {
    if (!line) return;
    history_init();

    /* Skip if line is only whitespace */
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') return;

    /* Skip consecutive duplicate */
    if (g_history.count > 0 && strcmp(g_history.entries[g_history.count - 1], line) == 0) {
        return;
    }

    if (g_history.count >= g_history.capacity) {
        g_history.capacity *= 2;
        g_history.entries = safe_realloc(g_history.entries, sizeof(char *) * g_history.capacity);
    }

    g_history.entries[g_history.count++] = safe_strdup(line);
}

const char *history_get(int index) {
    if (index < 0 || index >= g_history.count) return NULL;
    return g_history.entries[index];
}

int history_count(void) {
    return g_history.count;
}

void history_clear(void) {
    if (!g_history.entries) return;
    for (int i = 0; i < g_history.count; i++) {
        if (g_history.entries[i]) free(g_history.entries[i]);
        g_history.entries[i] = NULL;
    }
    g_history.count = 0;
}

void history_print(int limit) {
    if (g_history.count == 0) return;
    int start = 0;
    if (limit > 0 && limit < g_history.count) {
        start = g_history.count - limit;
    }
    for (int i = start; i < g_history.count; i++) {
        printf("%5d  %s\n", i + 1, g_history.entries[i]);
    }
}

void history_load(const char *filepath) {
    if (!filepath) return;
    history_init();
    FILE *fp = fopen(filepath, "r");
    if (!fp) return;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, fp)) != -1) {
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        if (line[0] != '\0') {
            history_add(line);
        }
    }
    free(line);
    fclose(fp);
}

void history_save(const char *filepath) {
    if (!filepath || g_history.count == 0) return;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return;

    for (int i = 0; i < g_history.count; i++) {
        fprintf(fp, "%s\n", g_history.entries[i]);
    }
    fclose(fp);
}
