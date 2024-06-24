#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR(type, vector)\
typedef struct vector {\
    size_t num_ ## type ## s;\
    type ## _t *type ## s;\
    size_t buffer_size;\
} vector ## _t;\
static inline void vector ## _init(vector ## _t *vector, size_t buffer_size) {\
    vector->num_ ## type ## s = 0;\
    vector->buffer_size = buffer_size;\
    vector->type ## s = calloc(buffer_size, sizeof(type ## _t));\
}\
static inline void vector ## _destroy(vector ## _t *vector) {\
    free(vector->type ## s);\
}\
static inline void vector ## _append(vector ## _t *vector, type ## _t t) {\
    if (vector->num_ ## type ## s >= vector->buffer_size) {\
        vector->buffer_size <<= 1;\
        type ## _t *buffer = calloc(vector->buffer_size, sizeof(*buffer));\
        memcpy(buffer, vector->type ## s, sizeof(*buffer) * (vector->buffer_size >> 1));\
        free(vector->type ## s);\
        vector->type ## s = buffer;\
    }\
    vector->type ## s[vector->num_ ## type ## s++] = t;\
}\
static inline void vector ## _ensure(vector ## _t *vector, size_t capacity) {\
    if (vector->buffer_size < capacity) {\
        vector->buffer_size = capacity;\
        type ## _t *buffer = calloc(capacity, sizeof(*buffer));\
        memcpy(buffer, vector->type ## s, sizeof(*buffer) * vector->num_ ## type ## s);\
        free(vector->type ## s);\
        vector->type ## s = buffer;\
    }\
}\
static inline void vector ## _trimTo(vector ## _t *vector, uint16_t index) {\
    memmove(&vector->type ## s[0], &vector->type ## s[index], (vector->num_ ## type ## s - index) * sizeof(type ## _t));\
    vector->num_ ## type ## s -= index;\
}\
static inline type ## _t vector ## _pop(vector ## _t *vector) {\
    return vector->type ## s[--vector->num_ ## type ## s];\
}
