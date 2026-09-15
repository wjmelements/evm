#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct data {
    size_t size;
    uint8_t *content;
} data_t;

#define CODE_PADDING 33 // PUSH32 tail plus next opcode

static inline uint8_t *allocPaddedCode(size_t size) {
    return calloc(size + CODE_PADDING, 1);
}

static inline data_t copyPaddedCode(data_t src) {
    data_t out;
    out.size = src.size;
    out.content = allocPaddedCode(src.size);
    if (src.size) {
        memcpy(out.content, src.content, src.size);
    }
    return out;
}

static inline void fprintData(FILE *file, data_t data) {
    for (size_t i = 0; i < data.size; i++) {
        fprintf(file, "%02x", data.content[i]);
    }
}

static inline int DataEqual(const data_t *expected, const data_t *actual) {
    return expected->size == actual->size && memcmp(expected->content, actual->content, actual->size) == 0;
}
