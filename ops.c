#include "ops.h"
#include <stdio.h>

const char *opnames = ""
#define OP(opcode,opname,argCount,retCount,gas) #opname " "
OPS
#undef OP
;


int main() {
    puts(opnames);
}

