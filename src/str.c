#include <string.h>

#include "str.h"

Str str_split(Str *str, char ch) {
    Str result = *str;
    const char *end = memchr(str->data, ch, str->size);

    if (end == NULL) {
        str->data += str->size;
        str->size = 0;
    } else {
        result.size = end - str->data;
        str->data += result.size + 1;
        str->size -= result.size + 1;
    }

    return result;
}
