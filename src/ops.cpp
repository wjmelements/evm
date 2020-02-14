#include "ops.h"

const uint8_t argCount[NUM_OPCODES] = {
    #define OP(name,in,out) in,
    OPS
    #undef OP
};
const uint8_t retCount[NUM_OPCODES] = {
    #define OP(name,in,out) out,
    OPS
    #undef OP
};
