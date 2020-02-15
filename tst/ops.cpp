#include "ops.h"
#include <assert.h>

#include <stdio.h>
int main() {
    assert(NUM_OPCODES == 256);
    assert(argCount[ADD] == 2);
    assert(retCount[ADD] == 1);
    assert(argCount[SELFDESTRUCT] == 1);
    #define OP(name,in,out) assert(opFromString(#name) == name);
    OPS
    #undef OP
    return 0;
}
