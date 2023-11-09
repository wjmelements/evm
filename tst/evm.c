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
    for (int i = 0; i < 31; i++) {
        assert(result.returnData.content[i] == 0xff);
    }
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

void test_spaghetti() {
    evmInit();

    address_t from;
    uint64_t gas;
    val_t value;
    data_t input;

    uint8_t program[] = {
        PUSH0,
        JUMPDEST,
        DUP1, ISZERO, PUSH1, 0x19, JUMPI,
        PC, MSIZE, MSTORE,
        MSIZE, CALLDATASIZE, REVERT,
        JUMPDEST,
        PUSH1, 0x01, JUMPI,
        JUMPDEST,
        DUP1, PUSH1, 0x0d, JUMPI,
        PUSH1, 0x01, ADD,
        JUMPDEST,
        DUP1, PUSH1, 0x02, GT,
        PUSH1, 0x11, JUMPI,
        STOP,
    };

    input.size = sizeof(program);
    input.content = program;

    result_t result = evmCreate(from, gas, value, input);
    evmFinalize();
    assert(UPPER(UPPER(result.status)) == 0);
    assert(UPPER(LOWER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0);
    assert(LOWER(LOWER(result.status)) == 0);
    assert(result.returnData.size == 32);
    for (int i = 0; i < 31; i++) {
        assert(result.returnData.content[i] == 0);
    }
    assert(result.returnData.content[31] == 7);
}
int main() {
    test_stop();
    test_mstoreReturn();
    test_math();
    test_xorSwap();
    test_spaghetti();
    return 0;
}
