#ifndef STR_H
#define STR_H

#include <stddef.h>

typedef struct {
  const char *data;
  size_t size;
} Str;

#define str_new(d, s) ((Str){.data = (d), .size = (s)})

Str str_split(Str *str, char ch);

#endif // STR_H
