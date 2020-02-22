#include "ops.h"
#include "label.h"
#include <stdlib.h>

#define STACKSIZE 1024
static op_t scanstack[STACKSIZE];
static op_t scanstackIndex = 0;
static label_t *labelstack[STACKSIZE];


static inline int scanstackEmpty() {
    return scanstackIndex == 0;
}

static inline void scanstackPush(op_t op) {
    scanstack[scanstackIndex++] = op;
}

static inline void scanstackPushLabel(const char *start, size_t len, op_t type) {
    label_t *label = (label_t *)malloc(sizeof(label_t));
    label->start = start;
    label->length = len;
    scanstackPush(type);
    labelstack[scanstackIndex] = label;
}

static inline op_t scanstackPop() {
    return scanstack[--scanstackIndex];
}

static inline int scanstackTopLabel(label_t *out) {
    if (labelstack[scanstackIndex]) {
        *out = *labelstack[scanstackIndex];
        free(labelstack[scanstackIndex]);
        labelstack[scanstackIndex] = 0;
        return 1;
    }
    return 0;
}
