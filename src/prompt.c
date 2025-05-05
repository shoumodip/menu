#include <ctype.h>
#include <stdbool.h>

#include "common.h"
#include "da.h"
#include "prompt.h"

void prompt_insert(Prompt *p, char ch) {
    da_append(p, ch);
    for (size_t i = p->count - 1; i > p->cursor; i--) {
        p->data[i] = p->data[i - 1];
    }
    p->data[p->cursor++] = ch;
}

void prompt_delete(Prompt *p, void (*motion)(Prompt *p)) {
    const size_t mark = p->cursor;
    motion(p);

    const size_t start = min(mark, p->cursor);
    const size_t end = max(mark, p->cursor);

    memmove(p->data + start, p->data + end, sizeof(*p->data) + (p->count - end));
    p->count -= end - start;
    p->cursor = start;
}

void prompt_start(Prompt *p) {
    p->cursor = 0;
}

void prompt_end(Prompt *p) {
    p->cursor = p->count;
}

void prompt_prev_char(Prompt *p) {
    if (p->cursor) {
        p->cursor--;
    }
}

void prompt_next_char(Prompt *p) {
    if (p->cursor < p->count) {
        p->cursor++;
    }
}

static bool is_word(char ch) {
    return isalnum(ch) || ch == '_';
}

void prompt_prev_word(Prompt *p) {
    if (p->cursor == 0) {
        return;
    }

    while (p->cursor > 0 && !is_word(p->data[p->cursor - 1])) {
        p->cursor--;
    }

    while (p->cursor > 0 && is_word(p->data[p->cursor - 1])) {
        p->cursor--;
    }
}

void prompt_next_word(Prompt *p) {
    if (p->cursor >= p->count) {
        return;
    }

    while (p->cursor < p->count && !is_word(p->data[p->cursor])) {
        p->cursor++;
    }

    while (p->cursor < p->count && is_word(p->data[p->cursor])) {
        p->cursor++;
    }
}
