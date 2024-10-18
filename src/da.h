#ifndef DA_H
#define DA_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define DA_INIT_CAP 128

#define DynamicArray(T)                                                                            \
    struct {                                                                                       \
        T *data;                                                                                   \
        size_t count;                                                                              \
        size_t capacity;                                                                           \
    }

#define da_free(l)                                                                                 \
    do {                                                                                           \
        free((l)->data);                                                                           \
        memset((l), 0, sizeof(*(l)));                                                              \
    } while (0)

#define da_append(l, v)                                                                            \
    do {                                                                                           \
        if ((l)->count >= (l)->capacity) {                                                         \
            (l)->capacity = (l)->capacity == 0 ? DA_INIT_CAP : (l)->capacity * 2;                  \
            (l)->data = realloc((l)->data, (l)->capacity * sizeof(*(l)->data));                    \
            assert((l)->data);                                                                     \
        }                                                                                          \
                                                                                                   \
        (l)->data[(l)->count++] = (v);                                                             \
    } while (0)

#define da_append_many(l, v, c)                                                                    \
    do {                                                                                           \
        if ((l)->count + (c) > (l)->capacity) {                                                    \
            if ((l)->capacity == 0) {                                                              \
                (l)->capacity = DA_INIT_CAP;                                                       \
            }                                                                                      \
                                                                                                   \
            while ((l)->count + (c) > (l)->capacity) {                                             \
                (l)->capacity *= 2;                                                                \
            }                                                                                      \
                                                                                                   \
            (l)->data = realloc((l)->data, (l)->capacity * sizeof(*(l)->data));                    \
            assert((l)->data);                                                                     \
        }                                                                                          \
                                                                                                   \
        if ((v) != NULL) {                                                                         \
            memcpy((l)->data + (l)->count, (v), (c) * sizeof(*(l)->data));                         \
            (l)->count += (c);                                                                     \
        }                                                                                          \
    } while (0)

#endif // DA_H
