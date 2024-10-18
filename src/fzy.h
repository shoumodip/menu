#ifndef FZY_H
#define FZY_H

#include "da.h"
#include "str.h"

typedef struct {
    Str str;
    double score;
    size_t *positions;
} Match;

typedef struct {
    DynamicArray(double) B;
    DynamicArray(double) D;
    DynamicArray(double) M;

    DynamicArray(Match) matches;
    DynamicArray(size_t) positions;
} Fzy;

void fzy_init(void);
void fzy_free(Fzy *f);
void fzy_filter(Fzy *f, Str needle, Str *items, size_t count);

#endif // FZY_H
