#include <stdint.h>
#include <string.h>
#include <stdio.h>

typedef struct data {
    size_t size;
    uint8_t *content;
} data_t;

static inline void fprintData(FILE* file, data_t data) {
    for (size_t i = 0; i < data.size; i++) {
        fprintf(file, "%02x", data.content[i]);
    }
}

static inline int DataEqual(const data_t *expected, const data_t *actual) {
    return expected->size == actual->size && memcmp(expected->content, actual->content, actual->size) == 0;
}
