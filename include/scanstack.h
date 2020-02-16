#include "ops.h"

static op_t scanstack[1024];
static op_t *scanscanstackHead = &scanstack[0];

static inline int scanstackEmpty() {
    return scanscanstackHead != &scanstack[0];
}

static inline void scanstackPush(op_t op) {
    *scanscanstackHead = op;
    scanscanstackHead++;
}

static inline op_t scanstackPop() {
    --scanscanstackHead;
    return *scanscanstackHead;
}
