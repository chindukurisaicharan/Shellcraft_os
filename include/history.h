#ifndef SHELLCRAFT_HISTORY_H
#define SHELLCRAFT_HISTORY_H

#include <stdbool.h>

void history_init(void);
void history_cleanup(void);

void history_add(const char *line);
const char *history_get(int index);
int history_count(void);
void history_clear(void);
void history_print(int limit);

void history_load(const char *filepath);
void history_save(const char *filepath);

#endif /* SHELLCRAFT_HISTORY_H */
