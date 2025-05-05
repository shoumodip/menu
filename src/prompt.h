#ifndef PROMPT_H
#define PROMPT_H

#include <stddef.h>

typedef struct {
    char  *data;
    size_t count;
    size_t cursor;
    size_t capacity;
} Prompt;

void prompt_insert(Prompt *p, char ch);
void prompt_delete(Prompt *p, void (*motion)(Prompt *p));

void prompt_start(Prompt *p);
void prompt_end(Prompt *p);

void prompt_prev_char(Prompt *p);
void prompt_next_char(Prompt *p);

void prompt_prev_word(Prompt *p);
void prompt_next_word(Prompt *p);

#endif // PROMPT_H
