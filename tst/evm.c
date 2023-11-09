#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "ops.h"
#include "evm.h"


void test_stop() {
    evmInit();

    address_t from;
    uint64_t gas;
    val_t value;
    data_t input;

    uint8_t program[] = {
        STOP,
    };

    input.size = sizeof(program);
    input.content = program;

    result_t result = evmCreate(from, gas, value, input);
    evmFinalize();
    // TODO result.status should instead be the return address
    assert(UPPER(UPPER(result.status)) == 0);
    assert(UPPER(LOWER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0);
    assert(LOWER(LOWER(result.status)) == 1);
    assert(result.returnData.size == 0);
}

void test_mstoreReturn() {
    evmInit();

    address_t from;
    uint64_t gas;
    val_t value;
    data_t input;

    uint8_t program[] = {
        PUSH32,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
        PUSH0,
        MSTORE,
        PUSH1, 0x20,
        PUSH0,
        RETURN,
    };

    input.size = sizeof(program);
    input.content = program;

    result_t result = evmCreate(from, gas, value, input);
    evmFinalize();
    // TODO result.status should instead be the return address
    assert(UPPER(UPPER(result.status)) == 0);
    assert(UPPER(LOWER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0);
    assert(LOWER(LOWER(result.status)) == 1);
    assert(result.returnData.size == 32);
    assert(0 == memcmp(result.returnData.content, program + 1, 32));
}
void test_math() {
    evmInit();

    address_t from;
    uint64_t gas;
    val_t value;
    data_t input;

    // 0 - (3 * 7 + 4 / 2)
    uint8_t program[] = {
        PUSH1, 0x02,
        PUSH1, 0x04,
        DIV,
        PUSH1, 0x07,
        PUSH1, 0x03,
        MUL,
        ADD,
        PUSH0,
        SUB,
        PUSH0,
        MSTORE,
        MSIZE,
        PUSH0,
        REVERT,
    };

    input.size = sizeof(program);
    input.content = program;

    result_t result = evmCreate(from, gas, value, input);
    evmFinalize();

    assert(zero256(&result.status));
    assert(result.returnData.size == 32);
    assert(result.returnData.content[0] == 0xff);
    assert(result.returnData.content[1] == 0xff);
    assert(result.returnData.content[2] == 0xff);
    assert(result.returnData.content[3] == 0xff);
    assert(result.returnData.content[4] == 0xff);
    assert(result.returnData.content[5] == 0xff);
    assert(result.returnData.content[6] == 0xff);
    assert(result.returnData.content[7] == 0xff);
    assert(result.returnData.content[8] == 0xff);
    assert(result.returnData.content[9] == 0xff);
    assert(result.returnData.content[10] == 0xff);
    assert(result.returnData.content[11] == 0xff);
    assert(result.returnData.content[12] == 0xff);
    assert(result.returnData.content[13] == 0xff);
    assert(result.returnData.content[14] == 0xff);
    assert(result.returnData.content[15] == 0xff);
    assert(result.returnData.content[16] == 0xff);
    assert(result.returnData.content[17] == 0xff);
    assert(result.returnData.content[18] == 0xff);
    assert(result.returnData.content[19] == 0xff);
    assert(result.returnData.content[20] == 0xff);
    assert(result.returnData.content[21] == 0xff);
    assert(result.returnData.content[22] == 0xff);
    assert(result.returnData.content[23] == 0xff);
    assert(result.returnData.content[24] == 0xff);
    assert(result.returnData.content[25] == 0xff);
    assert(result.returnData.content[26] == 0xff);
    assert(result.returnData.content[27] == 0xff);
    assert(result.returnData.content[28] == 0xff);
    assert(result.returnData.content[29] == 0xff);
    assert(result.returnData.content[30] == 0xff);
    assert(result.returnData.content[31] == 0xe9);
}

void test_xorSwap() {
    evmInit();

    address_t from;
    uint64_t gas;
    val_t value;
    data_t input;

    uint8_t program[] = {
        PUSH32, // b
        0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        PUSH32, // a
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
        0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
        DUP2, XOR, // a ^= b
        SWAP1, DUP2, XOR, SWAP1, // b ^= a
        DUP2, XOR, // a ^= b
        MSIZE, MSTORE,
        MSIZE, MSTORE,
        MSIZE, PUSH0, RETURN,
    };

    input.size = sizeof(program);
    input.content = program;

    result_t result = evmCreate(from, gas, value, input);
    evmFinalize();
    // TODO result.status should instead be the return address
    assert(UPPER(UPPER(result.status)) == 0);
    assert(UPPER(LOWER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0);
    assert(LOWER(LOWER(result.status)) == 1);
    assert(result.returnData.size == 64);
    assert(0 == memcmp(result.returnData.content, program + 1, 32));
    assert(0 == memcmp(result.returnData.content + 32, program + 34, 32));
}

int main() {
    test_stop();
    test_mstoreReturn();
    test_math();
    test_xorSwap();
    return 0;
}
