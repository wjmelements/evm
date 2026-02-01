#include "data.h"
#include "path.h"
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

op_t program[40];

void test_parseAssemble() {
    scanInit();
    const char *test = "CODECOPY(0, selfdestruct, #selfdestruct)\
        RETURN(0, #selfdestruct) \
        { selfdestruct: assemble tst/in/selfdestruct.evm }";
    const char *remaining = test;
    uint32_t programLength = 0;
    while (scanValid(&remaining)) {
        assert(programLength < 40);
        program[programLength++] = scanNextOp(&remaining);
    }
    scanFinalize(program, &programLength);

    op_t expected[] = {
        PUSH1, 0x02, PUSH1, 0x0a, PUSH0, CODECOPY,
        PUSH1, 0x02, PUSH0, RETURN,
        CALLER, SELFDESTRUCT,
    };
    assert(programLength == 12);
    assert(memcmp(program, expected, 12) == 0);
}

void test_parseConstruct() {
    scanInit();
    const char *test = "CODECOPY(0, test, #test)\
        RETURN(0, #test)\
        { test: construct tst/in/selfdestruct.evm }";
    const char *remaining = test;
    uint32_t programLength = 0;
    while (scanValid(&remaining)) {
        assert(programLength < 40);
        program[programLength++] = scanNextOp(&remaining);
    }
    scanFinalize(program, &programLength);

    op_t expected[] = {
        PUSH1, 0x0a, PUSH1, 0x0a, PUSH0, CODECOPY,
        PUSH1, 0x0a, PUSH0, RETURN,
        PUSH2, CALLER, SELFDESTRUCT, RETURNDATASIZE, MSTORE,
        PUSH1, 0x02, PUSH1, 0x1e, RETURN,
    };
    assert(programLength == 20);
    assert(memcmp(program, expected, 20) == 0);
}


int main() {
    pathInit("bin/evm");
    scanInit();

    const char *remaining;
    op_t op;
    const char *test0 = "GAS CODESIZE\nADD";
    remaining = test0;
    assert(scanNextOp(&remaining) == GAS);
    assert(scanNextOp(&remaining) == CODESIZE);
    assert(scanNextOp(&remaining) == ADD);
    assert(!scanValid(&remaining));

    scanInit();
    const char *test1 = "ADD(CODESIZE,GAS)";
    remaining = test1;
    assert(scanNextOp(&remaining) == GAS);
    assert(scanNextOp(&remaining) == CODESIZE);
    assert(scanNextOp(&remaining) == ADD);
    assert(!scanValid(&remaining));

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
    assert(!scanValid(&remaining));

    test_parseConstant();

    test_parseAssemble();
    test_parseConstruct();
    return 0;
}
