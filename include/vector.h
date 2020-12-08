#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR(type, vector)\
typedef struct vector {\
    uint32_t num_ ## type ## s;\
    uint32_t buffer_size;\
    type ## _t *type ## s;\
} vector ## _t;\
static inline void vector ## _init(vector ## _t *vector, uint32_t buffer_size) {\
    vector->num_ ## type ## s = 0;\
    vector->buffer_size = buffer_size;\
    vector->type ## s = calloc(buffer_size, sizeof(type ## _t));\
}\
static inline void vector ## _destroy(vector ## _t *vector) {\
    free(vector->type ## s);\
}\
static inline void vector ## _append(vector ## _t *vector, type ## _t type) {\
    if (vector->num_ ## type ## s >= vector->buffer_size) {\
        vector->buffer_size <<= 1;\
        type ## _t *buffer = calloc(vector->buffer_size, sizeof(*buffer));\
        memcpy(buffer, vector->type ## s, sizeof(*buffer) * (vector->buffer_size >> 1));\
        free(vector->type ## s);\
        vector->type ## s = buffer;\
    }\
    vector->type ## s[vector->num_ ## type ## s++] = type;\
}\
static inline void vector ## _trimTo(vector ## _t *vector, uint16_t index) {\
    memmove(&vector->type ## s[0], &vector->type ## s[index], (vector->num_ ## type ## s - index) * sizeof(type ## _t));\
    vector->num_ ## type ## s -= index;\
}\
static inline type ## _t vector ## _pop(vector ## _t *vector) {\
    return vector->type ## s[--vector->num_ ## type ## s];\
}
