#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR(type, vector) \
        typedef struct vector { \
            size_t num_ ## type ## s; \
            type ## _t *type ## s; \
            size_t buffer_size; \
        } vector ## _t; \
        static inline void vector ## _init(vector ## _t *vector, size_t buffer_size) { \
            vector->num_ ## type ## s = 0; \
            vector->buffer_size = buffer_size; \
            vector->type ## s = calloc(buffer_size, sizeof(type ## _t)); \
        } \
        static inline void vector ## _destroy(vector ## _t *vector) { \
            free(vector->type ## s); \
        } \
        static inline void vector ## _ensure(vector ## _t *vector, size_t capacity) { \
            if (vector->buffer_size < capacity) { \
                vector->type ## s = realloc(vector->type ## s, capacity * sizeof(type ## _t)); \
                memset(vector->type ## s + vector->num_ ## type ## s, 0, (capacity - vector->num_ ## type ## s) * sizeof(type ## _t)); \
                vector->buffer_size = capacity; \
            } \
        } \
        static inline void vector ## _grow(vector ## _t *vector, size_t capacity) { \
            if (vector->buffer_size < capacity) { \
                size_t doubled = vector->buffer_size << 1; \
                vector ## _ensure(vector, doubled > capacity ? doubled : capacity); \
            } \
        } \
        static inline void vector ## _append(vector ## _t *vector, type ## _t t) { \
            vector ## _grow(vector, vector->num_ ## type ## s + 1); \
            vector->type ## s[vector->num_ ## type ## s++] = t; \
        } \
        static inline void vector ## _extend(vector ## _t *vector, const type ## _t *items, size_t count) { \
            vector ## _grow(vector, vector->num_ ## type ## s + count); \
            memcpy(vector->type ## s + vector->num_ ## type ## s, items, count * sizeof(type ## _t)); \
            vector->num_ ## type ## s += count; \
        } \
        static inline void vector ## _trimTo(vector ## _t *vector, uint16_t index) { \
            memmove(&vector->type ## s[0], &vector->type ## s[index], (vector->num_ ## type ## s - index) * sizeof(type ## _t)); \
            vector->num_ ## type ## s -= index; \
        } \
        static inline type ## _t vector ## _pop(vector ## _t *vector) { \
            return vector->type ## s[--vector->num_ ## type ## s]; \
        }
