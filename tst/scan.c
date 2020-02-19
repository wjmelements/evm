#include "scan.h"
#include <assert.h>

extern op_t parseConstant(const char **iter);

void test_parseConstant() {
    //const char *dec32 = "32";
    const char *hex32 = "0x32";
    op_t op = parseConstant(&hex32);
    assert(op == PUSH1);
    op = scanNextOp(&hex32);
    assert(op == 0x32);
}


int main() {
    scanInit();

    const char *remaining;
    op_t op;
    const char *test0 = "GAS CODESIZE\nADD";
    remaining = test0;
    assert(scanNextOp(&remaining) == GAS);
    assert(scanNextOp(&remaining) == CODESIZE);
    assert(scanNextOp(&remaining) == ADD);

    scanInit();
    const char *test1 = "ADD(CODESIZE,GAS)";
    remaining = test1;
    assert(scanNextOp(&remaining) == GAS);
    assert(scanNextOp(&remaining) == CODESIZE);
    assert(scanNextOp(&remaining) == ADD);

    scanInit();
    const char *test2 = "ADD(MUL(CODESIZE,TIMESTAMP),SUB(GAS,CALLER))";
    remaining = test2;
    assert(scanNextOp(&remaining) == CALLER);
    assert(scanNextOp(&remaining) == GAS);
    assert(scanNextOp(&remaining) == SUB);
    assert(scanNextOp(&remaining) == TIMESTAMP);
    assert(scanNextOp(&remaining) == CODESIZE);
    assert(scanNextOp(&remaining) == MUL);
    assert(scanNextOp(&remaining) == ADD);

    test_parseConstant();
    return 0;
}
