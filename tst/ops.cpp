#include "ops.h"
#include <assert.h>

int main() {
    assert(NUM_OPCODES == 256);
    assert(argCount[ADD] == 2);
    assert(retCount[ADD] == 1);
    assert(argCount[SELFDESTRUCT] == 1);
    return 0;
}
