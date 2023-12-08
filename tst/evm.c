#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "ops.h"
#include "evm.h"


void test_stop() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 53006;
    val_t value;
    data_t input;

    op_t program[] = {
        STOP,
    };

    input.size = sizeof(program);
    input.content = program;

    result_t result = txCreate(from, gas, value, input);
    evmFinalize();
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == 0);
    assert(result.gasRemaining == 0);
}

void test_mstoreReturn() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 60044;
    val_t value;
    data_t input;

    op_t program[] = {
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

    result_t result = txCreate(from, gas, value, input);
    evmFinalize();
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == 32);
    assert(0 == memcmp(result.returnData.content, program + 1, 32));
    assert(result.gasRemaining == 0);
}
void test_math() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 72856;
    val_t value;
    data_t input;

    // 60026004046007600302015f035f5262ce378160021c595262abcdef601e1a5952595ffd
    op_t program[] = {
        // 0 - (3 * 7 + 4 / 2)
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
        // 0xce3781 >> 2
        PUSH3, 0xce, 0x37, 0x81, PUSH1, 0x02, SHR, MSIZE, MSTORE,
        PUSH3, 0xab, 0xcd, 0xef, PUSH1, 0x1e, BYTE, MSIZE, MSTORE,
        MSIZE,
        PUSH0,
        REVERT,
    };

    input.size = sizeof(program);
    input.content = program;

    result_t result = txCreate(from, gas, value, input);
    evmFinalize();

    assert(zero256(&result.status));
    assert(result.returnData.size == 96);
    for (int i = 0; i < 31; i++) {
        assert(result.returnData.content[i] == 0xff);
    }
    assert(result.returnData.content[31] == 0xe9);
    for (int i = 32; i < 61; i++) {
        assert(result.returnData.content[i] == 0x00);
    }
    // 0x338de0
    assert(result.returnData.content[61] == 0x33);
    assert(result.returnData.content[62] == 0x8d);
    assert(result.returnData.content[63] == 0xe0);
    for (int i = 64; i < 95; i++) {
        assert(result.returnData.content[i] == 0x00);
    }
    assert(result.returnData.content[95] == 0xcd);

    assert(result.gasRemaining == G_PER_CODEBYTE * result.returnData.size);
}

void test_shl() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 0x13540;
    val_t value;
    data_t input;

    // 5f60031b595260035f1b5952613377601d1b5952601d6133771b5952595ff3
    op_t program[] = {
        PUSH0, PUSH1, 3, SHL, MSIZE, MSTORE,
        PUSH1, 3, PUSH0, SHL, MSIZE, MSTORE,
        PUSH2, 0x33, 0x77, PUSH1, 29, SHL, MSIZE, MSTORE,
        PUSH1, 29, PUSH2, 0x33, 0x77, SHL, MSIZE, MSTORE,
        MSIZE, PUSH0, RETURN
    };

    input.content = program;
    input.size = sizeof(program);

    result_t result = txCreate(from, gas, value, input);

    op_t expected[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x6e, 0xe0, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    assert(result.returnData.size == sizeof(expected));
    assert(memcmp(result.returnData.content, expected, sizeof(expected)) == 0);

    evmFinalize();
}

void test_xorSwap() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 67152;
    val_t value;
    data_t input;

    op_t program[] = {
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

    result_t result = txCreate(from, gas, value, input);
    evmFinalize();
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == 64);
    assert(0 == memcmp(result.returnData.content, program + 1, 32));
    assert(0 == memcmp(result.returnData.content + 32, program + 34, 32));
    assert(result.gasRemaining == 0);
}

void test_sgtslt() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 0x14886;
    val_t value;
    data_t input;

    // tst/in/sgt.evm
    op_t program[] = {
        PUSH0,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        SGT, PUSH0, MSTORE8,
        PUSH0,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        SLT, PUSH1, 0x01, MSTORE8,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        PUSH0,
        SGT, PUSH1, 0x02, MSTORE8,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        PUSH1, 0x01,
        SLT, PUSH1, 0x03, MSTORE8,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        PUSH1, 0x01,
        SGT, PUSH1, 0x04, MSTORE8,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        PUSH0,
        SLT, PUSH1, 0x05, MSTORE8,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
        SGT, PUSH1, 0x06, MSTORE8,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
        SLT, PUSH1, 0x07, MSTORE8,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        SGT, PUSH1, 0x08, MSTORE8,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
        PUSH32,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        SLT, PUSH1, 0x09, MSTORE8,
        PUSH0,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        SGT, PUSH1, 0x0a, MSTORE8,
        PUSH0,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        SLT, PUSH1, 0x0b, MSTORE8,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        PUSH0,
        SGT, PUSH1, 0x0c, MSTORE8,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        PUSH0,
        SLT, PUSH1, 0x0d, MSTORE8,
        PUSH0,
        PUSH0,
        SGT, PUSH1, 0x0e, MSTORE8,
        PUSH0,
        PUSH0,
        SLT, PUSH1, 0x0f, MSTORE8,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        SGT, PUSH1, 0x10, MSTORE8,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        SLT, PUSH1, 0x11, MSTORE8,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        PUSH32,
        0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        SGT, MSIZE, MSTORE,
        PUSH32,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        PUSH32,
        0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        SLT, MSIZE, MSTORE,
        MSIZE, PUSH0, RETURN,
    };

    input.size = sizeof(program);
    input.content = program;

    result_t result = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    // 000101000100000101000001010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000000
    op_t expected[] = {
        0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    assert(result.returnData.size == sizeof(expected));
    assert(0 == memcmp(result.returnData.content, expected, sizeof(expected)));
    assert(result.gasRemaining == 0);


    evmFinalize();
}

void test_exp() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 0x18397;
    val_t value;
    data_t input;

    // tst/in/exp.evm
    op_t program[] = {
        PUSH0, PUSH1, 3, EXP, MSIZE, MSTORE,
        PUSH1, 3, PUSH1, 3, EXP, MSIZE, MSTORE,
        PUSH1, 3, PUSH1, 4, EXP, MSIZE, MSTORE,
        PUSH1, 4, PUSH1, 3, EXP, MSIZE, MSTORE,
        PUSH1, 32, PUSH1, 2, EXP, MSIZE, MSTORE,
        PUSH1, 160, PUSH1, 3, EXP, MSIZE, MSTORE,
        PUSH3, 0x0f, 0xff, 0xff, PUSH1, 0x1b, EXP, MSIZE, MSTORE,
        MSIZE, PUSH0, RETURN
    };

    input.content = program;
    input.size = sizeof(program);

    result_t result = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);

    op_t expected[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x30, 0x4d, 0x37, 0xf1, 0x20, 0xd6, 0x96, 0xc8, 0x34, 0x55, 0x0e, 0x63, 0xd9, 0xbb, 0x9c, 0x14,
        0xb4, 0xf9, 0x16, 0x5c, 0x9e, 0xde, 0x43, 0x4e, 0x46, 0x44, 0xe3, 0x99, 0x8d, 0x6d, 0xb8, 0x81,
        0xe1, 0xd0, 0xa3, 0xc6, 0xc0, 0x81, 0xea, 0x84, 0xc0, 0xfd, 0x20, 0xd5, 0x50, 0x42, 0x77, 0xc2,
        0xff, 0x1b, 0x79, 0xb5, 0xc2, 0xae, 0x76, 0xe4, 0x6b, 0xbb, 0xbd, 0xc2, 0xcd, 0x8b, 0xda, 0x13
    };
    assert(result.returnData.size == 7 * 32);
    for (size_t i = 0; i < result.returnData.size; i++) {
        if (result.returnData.content[i] != expected[i]) {
            fprintf(stderr, "%zu: %02x %02x\n", i, result.returnData.content[i], expected[i]);
        }
    }
    assert(memcmp(result.returnData.content, expected, 7 * 32) == 0);
    assert(result.gasRemaining == 0);

    evmFinalize();
}

void test_spaghetti() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 53690;
    val_t value;
    data_t input;

    // 5f5b80156019575859525936fd5b6001565b80600d576001015b8060021160115700
    op_t program[] = {
        PUSH0,
        JUMPDEST,
        DUP1, ISZERO, PUSH1, 0x19, JUMPI,
        PC, MSIZE, MSTORE,
        MSIZE, CALLDATASIZE, REVERT,
        JUMPDEST,
        PUSH1, 0x01, JUMP,
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

    result_t result = txCreate(from, gas, value, input);
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
    assert(result.gasRemaining == 0);
}

void test_sstore_sload() {
    evmInit();

    // 335f555f545952595ff3
    op_t program[] = {
        CALLER, PUSH0, SSTORE,
        PUSH0, SLOAD, MSIZE, MSTORE,
        MSIZE, PUSH0, RETURN,
    };
    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 81780;
    val_t value;
    data_t input;
    input.content = program;
    input.size = sizeof(program);

    result_t result = txCreate(from, gas, value, input);
    evmFinalize();
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == 32);
    for (int i = 0; i < 12; i++) {
        assert(result.returnData.content[i] == 0);
    }
    assert(memcmp(result.returnData.content + 12, from.address, 20) == 0);
    assert(result.gasRemaining == 0);
}

void test_sstore_refund() {
    evmInit();

    // 5f5f5560015f555f5f5500
    op_t program[] = {
        PUSH0, PUSH0, SSTORE,
        PUSH1, 0x01, PUSH0, SSTORE,
        PUSH0, PUSH0, SSTORE,
        STOP,
    };
    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 77680;
    val_t value;
    data_t input;
    input.content = program;
    input.size = sizeof(program);

    result_t result = txCreate(from, gas, value, input);
    evmFinalize();
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == 0);
    assert(result.gasRemaining == 17296);
}

void test_sstore_gauntlet() {
    evmInit();

    // 5f5f5560015f5560025f555f5f5560015f55600160015560026001555f60015500
    op_t program[] = {
        PUSH0, PUSH0, SSTORE,
        PUSH1, 0x01, PUSH0, SSTORE,
        PUSH1, 0x02, PUSH0, SSTORE,
        PUSH0, PUSH0, SSTORE,
        PUSH1, 0x01, PUSH0, SSTORE,
        PUSH1, 0x01, PUSH1, 0x01, SSTORE,
        PUSH1, 0x02, PUSH1, 0x01, SSTORE,
        PUSH0, PUSH1, 0x01, SSTORE,
        STOP,
    };
    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 120461;
    val_t value;
    data_t input;
    input.content = program;
    input.size = sizeof(program);

    result_t result = txCreate(from, gas, value, input);
    evmFinalize();
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == 0);
    assert(result.gasRemaining == 25853);
}

void test_selfbalance() {
    evmInit();

    // 30315952475952595ff3
    op_t program[] = {
        ADDRESS, BALANCE, MSIZE, MSTORE,
        SELFBALANCE, MSIZE, MSTORE,
        MSIZE, PUSH0, RETURN,
    };
    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 66089;
    val_t value;
    value[0] = 0xdd;
    value[1] = 0xee;
    value[2] = 0xff;
    data_t input;
    input.content = program;
    input.size = sizeof(program);


    evmMockBalance(from, value);
    result_t result = txCreate(from, gas, value, input);
    evmFinalize();
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == 64);
    for (int i = 0; i < 63; i++) {
        if (i % 32 == 31) {
            assert(result.returnData.content[i] == 0xff);
        } else if (i % 32 == 27) {
            assert(result.returnData.content[i] == 0xee);
        } else if (i % 32 == 23) {
            assert(result.returnData.content[i] == 0xdd);
        } else {
            assert(result.returnData.content[i] == 0x00);
        }
    }
    assert(result.gasRemaining == 0);
}
void test_callEmpty() {
    // 70a082310000000000000000000000004a6f6b9ff1fc974096f9063a45fd12bd5b928ad1
    evmInit();
    op_t callData[] = {
        0x70, 0xa0, 0x82, 0x31,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x4a, 0x6f, 0x6b, 0x9f, 0xf1,
        0xfc, 0x97, 0x40, 0x96, 0xf9,
        0x06, 0x3a, 0x45, 0xfd, 0x12,
        0xbd, 0x5b, 0x92, 0x8a, 0xd1,
    };
    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    address_t to = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 21432;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;
    data_t input;
    input.content = callData;
    input.size = sizeof(callData);

    result_t result = txCall(from, gas, to, value, input, NULL);
    evmFinalize();

    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0);
    assert(UPPER(LOWER(result.status)) == 0);
    assert(LOWER(LOWER(result.status)) == 1);
    assert(result.returnData.size == 0);
    assert(result.gasRemaining == 0);
}
void test_callBounce() {
    evmInit();

    // 385f5f394759525959595f34335af16015573d5ffd5b475952595ff3
    op_t program[] = {
        CODESIZE, PUSH0, PUSH0, CODECOPY,
        SELFBALANCE, MSIZE, MSTORE,
        MSIZE, MSIZE, MSIZE, PUSH0, CALLVALUE, CALLER, GAS, CALL,
        PUSH1, 0x15, JUMPI,
        RETURNDATASIZE, PUSH0, REVERT,
        JUMPDEST,
        SELFBALANCE, MSIZE, MSTORE,
        MSIZE, PUSH0, RETURN,
    };
    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 92329;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0xff;

    evmMockBalance(from, value);

    data_t input;
    input.content = program;
    input.size = sizeof(program);

    result_t result = txCreate(from, gas, value, input);
    evmFinalize();
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == 160);
    assert(memcmp(result.returnData.content, program, 28) == 0);
    for (int i = 28; i < 63; i++) {
        assert(result.returnData.content[i] == 0x00);
    }
    assert(result.returnData.content[63] == 0xff);
    for (int i = 64; i < 160; i++) {
        assert(result.returnData.content[i] == 0x00);
    }
    assert(result.gasRemaining == 0);
}
void test_extcodecopy() {
    evmInit();


    // 363d3d37363df3
#define PROGRAM_ECHO \
        CALLDATASIZE, RETURNDATASIZE, RETURNDATASIZE, CALLDATACOPY, \
        CALLDATASIZE, RETURNDATASIZE, RETURN
    op_t echo[] = {
        PROGRAM_ECHO
    };
    // 66363d3d37363df33d5260076019f3
    op_t createEcho[] = {
        PUSH7, PROGRAM_ECHO, RETURNDATASIZE, MSTORE,
        PUSH1, 7, PUSH1, 25, RETURN
    };
#undef PROGRAM_ECHO
    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 54659;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;

    data_t input;
    input.content = createEcho;
    input.size = sizeof(createEcho);

    result_t createResult = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(createResult.status)) == 0);
    assert(LOWER(UPPER(createResult.status)) == 0x80d9b122);
    assert(UPPER(LOWER(createResult.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(createResult.status)) == 0x010ffe7e38d227c3);
    assert(createResult.returnData.size == sizeof(echo));
    assert(memcmp(createResult.returnData.content, echo, sizeof(echo)) == 0);
    assert(createResult.gasRemaining == 0);

    gas = 57035;
    // 385952385f59395f353b5f595f353c595ff3
#define PROGRAM_EXTCODECOPY \
    CODESIZE, MSIZE, MSTORE, \
    CODESIZE, PUSH0, MSIZE, CODECOPY, \
    PUSH0, CALLDATALOAD, EXTCODESIZE, PUSH0, MSIZE, PUSH0, CALLDATALOAD, EXTCODECOPY, \
    MSIZE, PUSH0, RETURN
    op_t extcodecopy[] = {
        PROGRAM_EXTCODECOPY
    };
    // 71385952385f59395f353b5f595f353c595ff33d526012600ef3
    op_t createExtCodeCopy[] = {
        PUSH18, PROGRAM_EXTCODECOPY,
        RETURNDATASIZE, MSTORE,
        PUSH1, 18, PUSH1, 14, RETURN
    };
#undef PROGRAM_EXTCODECOPY
    input.size = sizeof(createExtCodeCopy);
    input.content = createExtCodeCopy;
    result_t secondCreateResult = txCreate(from, gas, value, input);
    assert(memcmp(secondCreateResult.returnData.content, extcodecopy, sizeof(extcodecopy)) == 0);
    assert(UPPER(UPPER(secondCreateResult.status)) == 0);
    assert(LOWER(UPPER(secondCreateResult.status)) == 0x47784b21);
    assert(UPPER(LOWER(secondCreateResult.status)) == 0x780d1e1d5efcd005);
    assert(LOWER(LOWER(secondCreateResult.status)) == 0xc5fe542813b6d71e);
    assert(secondCreateResult.gasRemaining == 0);

    // ensure upper bytes of upcoming calldata are zero
    address_t locations[4];
    bzero(locations, sizeof(locations));
    address_t echoAddress = locations[1] = AddressFromUint256(&createResult.status);
    address_t extcodecopyAddress = locations[3] = AddressFromUint256(&secondCreateResult.status);

    address_t to = extcodecopyAddress;

    input.size = 32;

    gas = 24117;
    input.content = locations[1].address - 12;
    result_t examineFirstAccount = txCall(from, gas, to, value, input, NULL);
    assert(UPPER(UPPER(examineFirstAccount.status)) == 0);
    assert(LOWER(UPPER(examineFirstAccount.status)) == 0);
    assert(UPPER(LOWER(examineFirstAccount.status)) == 0);
    assert(LOWER(LOWER(examineFirstAccount.status)) == 1);
    assert(examineFirstAccount.returnData.size == 96);
    for (int i = 0; i < 31; i++) {
        assert(examineFirstAccount.returnData.content[i] == 0);
    }
    assert(examineFirstAccount.returnData.content[31] == sizeof(extcodecopy));
    assert(memcmp(examineFirstAccount.returnData.content+32, extcodecopy, sizeof(extcodecopy)) == 0);
    for (int i = 32 + sizeof(extcodecopy); i < 64; i++) {
        assert(examineFirstAccount.returnData.content[i] == 0);
    }
    assert(memcmp(examineFirstAccount.returnData.content+64, echo, sizeof(echo)) == 0);
    for (int i = 64 + sizeof(echo); i < 96; i++) {
        assert(examineFirstAccount.returnData.content[i] == 0);
    }
    assert(examineFirstAccount.gasRemaining == 0);

    gas = 21617;
    input.content = locations[3].address - 12;
    result_t examineSecondAccount = txCall(from, gas, to, value, input, NULL);
    assert(UPPER(UPPER(examineSecondAccount.status)) == 0);
    assert(LOWER(UPPER(examineSecondAccount.status)) == 0);
    assert(UPPER(LOWER(examineSecondAccount.status)) == 0);
    assert(LOWER(LOWER(examineSecondAccount.status)) == 1);
    assert(examineSecondAccount.returnData.size == 96);
    for (int i = 0; i < 31; i++) {
        assert(examineSecondAccount.returnData.content[i] == 0);
    }
    assert(examineSecondAccount.returnData.content[31] == sizeof(extcodecopy));
    assert(memcmp(examineSecondAccount.returnData.content+32, extcodecopy, sizeof(extcodecopy)) == 0);
    for (int i = 32 + sizeof(extcodecopy); i < 64; i++) {
        assert(examineSecondAccount.returnData.content[i] == 0);
    }
    assert(memcmp(examineSecondAccount.returnData.content+64, extcodecopy, sizeof(extcodecopy)) == 0);
    for (int i = 64 + sizeof(extcodecopy); i < 96; i++) {
        assert(examineSecondAccount.returnData.content[i] == 0);
    }
    assert(examineSecondAccount.gasRemaining == 0);

    evmFinalize();
}

void test_deepCall() {
    evmInit();
    // 3d3580600757005b7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff01805952590259595f34305af1603c57595ffd5b595ff3
#define PROGRAM_DIVE \
    RETURNDATASIZE, CALLDATALOAD, \
    DUP1, PUSH1, 0x07, JUMPI, STOP, JUMPDEST, \
    PUSH32, \
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
    ADD, \
    DUP1, MSIZE, MSTORE, \
    MSIZE, MUL, MSIZE, MSIZE, PUSH0, CALLVALUE, ADDRESS, GAS, CALL, \
    PUSH1, 60, JUMPI, \
    MSIZE, PUSH0, REVERT, \
    JUMPDEST, \
    MSIZE, PUSH0, RETURN

    op_t dive[] = {
        PROGRAM_DIVE
    };
    // 60408060093d393df33d3580600757005b7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff01805952590259595f34305af1603c57595ffd5b595ff3
    op_t createDive[] = {
        PUSH1, 0x40, DUP1, PUSH1, 0x09, RETURNDATASIZE, CODECOPY, RETURNDATASIZE, RETURN,
        PROGRAM_DIVE
    };

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 66990;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;

    data_t input;
    input.content = createDive;
    input.size = sizeof(createDive);
    result_t createResult = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(createResult.status)) == 0);
    assert(LOWER(UPPER(createResult.status)) == 0x80d9b122);
    assert(UPPER(LOWER(createResult.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(createResult.status)) == 0x010ffe7e38d227c3);
    assert(createResult.returnData.size == sizeof(dive));
    assert(memcmp(createResult.returnData.content, dive, sizeof(dive)) == 0);
    assert(createResult.gasRemaining == 0);

    uint8_t param[32];
    bzero(param, 32);
    param[30] = 0x01;
    param[31] = 0xc8;
    address_t to = AddressFromUint256(&createResult.status);
    gas = 29968466;

    input.content = param;
    input.size = sizeof(param);
    result_t diveResult = txCall(from, gas, to, value, input, NULL);

    assert(UPPER(UPPER(diveResult.status)) == 0);
    assert(LOWER(UPPER(diveResult.status)) == 0);
    assert(UPPER(LOWER(diveResult.status)) == 0);
    assert(LOWER(LOWER(diveResult.status)) == 1);
    assert(diveResult.gasRemaining == 29494085);
    assert(diveResult.returnData.size == 32 * 0x1c8);

    uint256_t expected;
    readu256BE(param, &expected);
    uint256_t one;
    clear256(&one);
    LOWER(LOWER(one)) = 1;
    uint256_t actual;
    uint8_t *current = diveResult.returnData.content;
    while (!zero256(&expected)) {
        minus256(&expected, &one, &expected);
        readu256BE(current, &actual);
        assert(equal256(&expected, &actual));
        current += 32;
    }

    evmFinalize();
}

void test_revertStorage() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 67656;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0xffeeeedd;

    evmMockBalance(from, value);

#define PROGRAM_REVERTSSTORE \
        CALLER, ADDRESS, XOR, PUSH1, 15, JUMPI, \
        PUSH1, 32, CALLDATALOAD, PUSH0, CALLDATALOAD, SSTORE, \
        PUSH0, PUSH0, REVERT, \
        JUMPDEST, \
        CALLDATASIZE, PUSH0, PUSH0, CALLDATACOPY, \
        PUSH0, PUSH0, MSIZE, PUSH0, PUSH0, ADDRESS, GAS, CALL, MSIZE, MSTORE, \
        SELFBALANCE, MSIZE, MSTORE, \
        PUSH0, PUSH0, MSIZE, PUSH0, CALLVALUE, ADDRESS, GAS, CALL, MSIZE, MSTORE, \
        SELFBALANCE, MSIZE, MSTORE, \
        PUSH0, PUSH0, MSIZE, PUSH0, SELFBALANCE, ADDRESS, GAS, CALL, MSIZE, MSTORE, \
        SELFBALANCE, MSIZE, MSTORE, \
        PUSH0, CALLDATALOAD, SLOAD, MSIZE, MSTORE, \
        MSIZE, PUSH0, RETURN
    // 333018600f576020355f35555f5ffd5b365f5f375f5f595f5f305af159524759525f5f595f34305af159524759525f5f595f47305af159524759525f35545952595ff3
    op_t revertTest[] = {
        PROGRAM_REVERTSSTORE
    };
    // 60438060093d393df3333018600f576020355f35555f5ffd5b365f5f375f5f595f5f305af159524759525f5f595f34305af159524759525f5f595f47305af159524759525f35545952595ff3
    op_t createRevertTest[] = {
        PUSH1, 67, DUP1, PUSH1, 0x09, RETURNDATASIZE, CODECOPY, RETURNDATASIZE, RETURN,
        PROGRAM_REVERTSSTORE
    };
#undef PROGRAM_REVERTSSTORE
    data_t input;
    input.content = createRevertTest;
    input.size = sizeof(createRevertTest);
    result_t createResult = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(createResult.status)) == 0);
    assert(LOWER(UPPER(createResult.status)) == 0x80d9b122);
    assert(UPPER(LOWER(createResult.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(createResult.status)) == 0x010ffe7e38d227c3);
    assert(createResult.returnData.size == sizeof(revertTest));
    assert(memcmp(createResult.returnData.content, revertTest, sizeof(revertTest)) == 0);
    assert(createResult.gasRemaining == 0);

    value[1] = 0x22;
    value[2] = 0;
    evmMockBalance(from, value);

    input.size = 64;
    input.content = calloc(1, 64);
    input.content[63] = 0x01;

    gas = 103640;
    address_t to = AddressFromUint256(&createResult.status);

    result_t sstoreRevertResult = txCall(from, gas, to, value, input, NULL);
    assert(UPPER(UPPER(sstoreRevertResult.status)) == 0);
    assert(LOWER(UPPER(sstoreRevertResult.status)) == 0);
    assert(UPPER(LOWER(sstoreRevertResult.status)) == 0);
    assert(LOWER(LOWER(sstoreRevertResult.status)) == 1);
    assert(sstoreRevertResult.returnData.size == 288);
    assert(memcmp(input.content, sstoreRevertResult.returnData.content, input.size) == 0);
    for (int j = 0; j < 3; j++) {
        int i, k = j * 64;
        for (i = 64 + k; i < 123 + k; i++) {
            assert(sstoreRevertResult.returnData.content[i] == 0);
        }
        assert(sstoreRevertResult.returnData.content[i++] == 0x22);
        assert(sstoreRevertResult.returnData.content[i++] == 0xff);
        assert(sstoreRevertResult.returnData.content[i++] == 0xee);
        assert(sstoreRevertResult.returnData.content[i++] == 0xee);
        assert(sstoreRevertResult.returnData.content[i++] == 0xdd);
    }
    for (int i = 256; i < 288; i++) {
        assert(sstoreRevertResult.returnData.content[i] == 0);
    }
    assert(sstoreRevertResult.gasRemaining == 0);

    evmFinalize();
}

void test_revertSload() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 62464;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;

#define PROGRAM_REVERTSLOAD \
        CALLER, ADDRESS, XOR, PUSH1, 13, JUMPI, \
        PUSH0, SLOAD, MSIZE, MSTORE, \
        MSIZE, PUSH0, REVERT, \
        JUMPDEST, \
        PUSH1, 32, MSIZE, PUSH0, PUSH0, PUSH0, ADDRESS, GAS, CALL, MSIZE, MSTORE, \
        PUSH1, 32, MSIZE, PUSH0, PUSH0, PUSH0, ADDRESS, GAS, CALL, MSIZE, MSTORE, \
        PUSH0, SLOAD, MSIZE, MSTORE, \
        MSIZE, PUSH0, RETURN
    // 333018600d575f545952595ffd5b6020595f5f5f305af159526020595f5f5f305af159525f545952595ff3
    op_t revertTest[] = {
        PROGRAM_REVERTSLOAD
    };
    // 602b8060093d393df3333018600d575f545952595ffd5b6020595f5f5f305af159526020595f5f5f305af159525f545952595ff3
    op_t createRevertTest[] = {
        PUSH1, 43, DUP1, PUSH1, 0x09, RETURNDATASIZE, CODECOPY, RETURNDATASIZE, RETURN,
        PROGRAM_REVERTSLOAD
    };
#undef PROGRAM_REVERTSLOAD
    data_t input;
    input.content = createRevertTest;
    input.size = sizeof(createRevertTest);
    result_t createResult = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(createResult.status)) == 0);
    assert(LOWER(UPPER(createResult.status)) == 0x80d9b122);
    assert(UPPER(LOWER(createResult.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(createResult.status)) == 0x010ffe7e38d227c3);
    assert(createResult.returnData.size == sizeof(revertTest));
    assert(memcmp(createResult.returnData.content, revertTest, sizeof(revertTest)) == 0);
    assert(createResult.gasRemaining == 0);

    input.size = 0;

    gas = 27655;
    address_t to = AddressFromUint256(&createResult.status);

    result_t sloadRevertResult = txCall(from, gas, to, value, input, NULL);
    assert(UPPER(UPPER(sloadRevertResult.status)) == 0);
    assert(LOWER(UPPER(sloadRevertResult.status)) == 0);
    assert(UPPER(LOWER(sloadRevertResult.status)) == 0);
    assert(LOWER(LOWER(sloadRevertResult.status)) == 1);
    assert(sloadRevertResult.returnData.size == 160);
    for (int i = 0; i < 160; i++) {
        assert(sloadRevertResult.returnData.content[i] == 0);
    }
    assert(sloadRevertResult.gasRemaining == 0);

    evmFinalize();
}


// 0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3
#define ACCOUNT0 0x80, 0xd9, 0xb1, 0x22, 0xdc, 0x3a, 0x16, 0xfd, 0xc4, 0x1f, 0x96, 0xcf, 0x01, 0x0f, 0xfe, 0x7e, 0x38, 0xd2, 0x27, 0xc3
// 0x47784b21780d1e1d5efcd005c5fe542813b6d71e
#define ACCOUNT1 0x47, 0x78, 0x4b, 0x21, 0x78, 0x0d, 0x1e, 0x1d, 0x5e, 0xfc, 0xd0, 0x05, 0xc5, 0xfe, 0x54, 0x28, 0x13, 0xb6, 0xd7, 0x1e

void test_staticcallSstore() {
    evmInit();

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 58103;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;


    // 60213610600e576020355f3555005b5f35545f52595ff3
#define PROGRAM_STORAGE \
        PUSH1, 33, CALLDATASIZE, LT, PUSH1, 14, JUMPI, \
        PUSH1, 32, CALLDATALOAD, PUSH0, CALLDATALOAD, SSTORE, \
        STOP, \
        JUMPDEST, \
        PUSH0, CALLDATALOAD, SLOAD, PUSH0, MSTORE, \
        MSIZE, PUSH0, RETURN
    op_t storageTester[] = {
        PROGRAM_STORAGE
    };

    // 7660213610600e576020355f3555005b5f35545f52595ff33d5260176009f3
    op_t constructStorageTester[] = {
        PUSH23, PROGRAM_STORAGE, RETURNDATASIZE, MSTORE,
        PUSH1, 23, PUSH1, 9, RETURN
    };
#undef PROGRAM_STORAGE

    data_t input;
    input.size = sizeof(constructStorageTester);
    input.content = constructStorageTester;

    result_t result = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == sizeof(storageTester));
    assert(memcmp(result.returnData.content, storageTester, sizeof(storageTester)) == 0);
    assert(result.gasRemaining == 0);

    address_t storageTest = AddressFromUint256(&result.status);

    // 365f5f37595f365f7380d9b122dc3a16fdc41f96cf010ffe7e38d227c35afa602557595ffd5b595ff3
#define PROGRAM_STATICCALL \
        CALLDATASIZE, PUSH0, PUSH0, CALLDATACOPY, \
        MSIZE, PUSH0, CALLDATASIZE, PUSH0, PUSH20, ACCOUNT0, GAS, STATICCALL, \
        PUSH1, 37, JUMPI, \
        MSIZE, PUSH0, REVERT, \
        JUMPDEST, MSIZE, PUSH0, RETURN

    op_t staticCaller[] = {
        PROGRAM_STATICCALL
    };
    // 60298060093d393df3365f5f37595f365f7380d9b122dc3a16fdc41f96cf010ffe7e38d227c35afa602557595ffd5b595ff3
    op_t createStaticCaller[] = {
        PUSH1, 41, DUP1, PUSH1, 9, RETURNDATASIZE, CODECOPY, RETURNDATASIZE, RETURN,
        PROGRAM_STATICCALL
    };

    input.size = sizeof(createStaticCaller);
    input.content = createStaticCaller;
    gas = 0xf250;
    result = txCreate(from, gas, value, input);
    // 0x47784b21780d1e1d5efcd005c5fe542813b6d71e
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x47784b21);
    assert(UPPER(LOWER(result.status)) == 0x780d1e1d5efcd005);
    assert(LOWER(LOWER(result.status)) == 0xc5fe542813b6d71e);
    assert(result.returnData.size == sizeof(staticCaller));
    assert(memcmp(result.returnData.content, staticCaller, sizeof(staticCaller)) == 0);
    assert(result.gasRemaining == 0);

    address_t staticCall = AddressFromUint256(&result.status);
    op_t params[64];
    bzero(params, 64);
    input.content = params;

    // STATICCALL into SLOAD, returning 0
    input.size = 0;
    gas = 0x64c2;
    result = txCall(from, gas, staticCall, value, input, NULL);
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0);
    assert(UPPER(LOWER(result.status)) == 0);
    assert(LOWER(LOWER(result.status)) == 1);
    assert(result.returnData.size == 0);
    assert(result.gasRemaining == 15);

    // STATICCALL into SLOAD, returning 32
    input.size = 32;
    gas = 25928;
    result = txCall(from, gas, staticCall, value, input, NULL);
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0);
    assert(UPPER(LOWER(result.status)) == 0);
    assert(LOWER(LOWER(result.status)) == 1);
    assert(result.returnData.size == 32);
    assert(memcmp(result.returnData.content, params, result.returnData.size) == 0);
    assert(result.gasRemaining == 15);

    // SSTORE directly; success
    input.size = 64;
    gas = 23589;
    result = txCall(from, gas, storageTest, value, input, NULL);
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0);
    assert(UPPER(LOWER(result.status)) == 0);
    assert(LOWER(LOWER(result.status)) == 1);
    assert(result.returnData.size == 0);
    assert(result.gasRemaining == 101);

    // STATICCALL into SSTORE; forbidden
    gas = 0x10000;
    result = txCall(from, gas, staticCall, value, input, NULL);
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0);
    assert(UPPER(LOWER(result.status)) == 0);
    assert(LOWER(LOWER(result.status)) == 0);
    assert(result.returnData.size == 64);
    assert(result.gasRemaining == 633);

    evmFinalize();
}

void test_log() {
    evmInit();

    // 6e66363d3d37363df33d5260076019f35f52595fa05a595fa15a5a595fa25a5a5a595fa35a5a5a5a595fa4600f6011f3
    op_t logTest[] = {
        PUSH15, 0x66, 0x36, 0x3d, 0x3d, 0x37, 0x36, 0x3d, 0xf3, 0x3d, 0x52, 0x60, 0x07, 0x60, 0x19, 0xf3, PUSH0, MSTORE,
        MSIZE, PUSH0, LOG0,
        GAS, MSIZE, PUSH0, LOG1,
        GAS, GAS, MSIZE, PUSH0, LOG2,
        GAS, GAS, GAS, MSIZE, PUSH0, LOG3,
        GAS, GAS, GAS, GAS, MSIZE, PUSH0, LOG4,
        PUSH1, 15, PUSH1, 17, RETURN
    };

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 0xf8f6;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;
    data_t input;

    input.size = sizeof(logTest);
    input.content = logTest;

    result_t result = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);

    assert(result.stateChanges != NULL);
    stateChanges_t *stateChanges = result.stateChanges;
    assert(stateChanges->logChanges != NULL);

    logChanges_t *log4 = stateChanges->logChanges;
    assert(log4->topicCount == 4);
    assert(log4->prev != NULL);
    assert(log4->data.size == 32);
    assert(memcmp(log4->data.content + 17, logTest + 1, 15) == 0);
    assert(UPPER(UPPER(log4->topics[0])) == 0);
    assert(LOWER(UPPER(log4->topics[0])) == 0);
    assert(UPPER(LOWER(log4->topics[0])) == 0);
    assert(LOWER(LOWER(log4->topics[0])) == 0x141b);
    assert(UPPER(UPPER(log4->topics[1])) == 0);
    assert(LOWER(UPPER(log4->topics[1])) == 0);
    assert(UPPER(LOWER(log4->topics[1])) == 0);
    assert(LOWER(LOWER(log4->topics[1])) == 0x1419);
    assert(UPPER(UPPER(log4->topics[2])) == 0);
    assert(LOWER(UPPER(log4->topics[2])) == 0);
    assert(UPPER(LOWER(log4->topics[2])) == 0);
    assert(LOWER(LOWER(log4->topics[2])) == 0x1417);
    assert(UPPER(UPPER(log4->topics[3])) == 0);
    assert(LOWER(UPPER(log4->topics[3])) == 0);
    assert(UPPER(LOWER(log4->topics[3])) == 0);
    assert(LOWER(LOWER(log4->topics[3])) == 0x1415);

    logChanges_t *log3 = log4->prev;
    assert(log3->topicCount == 3);
    assert(log3->prev != NULL);
    assert(log3->data.size == 32);
    assert(memcmp(log3->data.content + 17, logTest + 1, 15) == 0);
    assert(UPPER(UPPER(log3->topics[0])) == 0);
    assert(LOWER(UPPER(log3->topics[0])) == 0);
    assert(UPPER(LOWER(log3->topics[0])) == 0);
    assert(LOWER(LOWER(log3->topics[0])) == 0x1b01);
    assert(UPPER(UPPER(log3->topics[1])) == 0);
    assert(LOWER(UPPER(log3->topics[1])) == 0);
    assert(UPPER(LOWER(log3->topics[1])) == 0);
    assert(LOWER(LOWER(log3->topics[1])) == 0x1aff);
    assert(UPPER(UPPER(log3->topics[2])) == 0);
    assert(LOWER(UPPER(log3->topics[2])) == 0);
    assert(UPPER(LOWER(log3->topics[2])) == 0);
    assert(LOWER(LOWER(log3->topics[2])) == 0x1afd);

    logChanges_t *log2 = log3->prev;
    assert(log2->topicCount == 2);
    assert(log2->prev != NULL);
    assert(log2->data.size == 32);
    assert(memcmp(log2->data.content + 17, logTest + 1, 15) == 0);
    assert(UPPER(UPPER(log2->topics[0])) == 0);
    assert(LOWER(UPPER(log2->topics[0])) == 0);
    assert(UPPER(LOWER(log2->topics[0])) == 0);
    assert(LOWER(LOWER(log2->topics[0])) == 0x206e);
    assert(UPPER(UPPER(log2->topics[1])) == 0);
    assert(LOWER(UPPER(log2->topics[1])) == 0);
    assert(UPPER(LOWER(log2->topics[1])) == 0);
    assert(LOWER(LOWER(log2->topics[1])) == 0x206c);

    logChanges_t *log1 = log2->prev;
    assert(log1->topicCount == 1);
    assert(log1->prev != NULL);
    assert(log1->data.size == 32);
    assert(memcmp(log1->data.content + 17, logTest + 1, 15) == 0);
    assert(UPPER(UPPER(log1->topics[0])) == 0);
    assert(LOWER(UPPER(log1->topics[0])) == 0);
    assert(UPPER(LOWER(log1->topics[0])) == 0);
    assert(LOWER(LOWER(log1->topics[0])) == 0x2462);

    logChanges_t *log0 = log1->prev;
    assert(log0->topicCount == 0);
    assert(log0->prev == NULL);
    assert(log0->topics == NULL);
    assert(log0->data.size == 32);
    assert(memcmp(log0->data.content + 17, logTest + 1, 15) == 0);

    assert(result.gasRemaining == 0);

    evmFinalize();
}

void test_sha3() {
    evmInit();

    // 595f205952595f205952596020205952595ff3
    op_t sha3Test[] = {
        MSIZE, PUSH0, SHA3, MSIZE, MSTORE,
        MSIZE, PUSH0, SHA3, MSIZE, MSTORE,
        MSIZE, PUSH1, 32, SHA3, MSIZE, MSTORE,
        MSIZE, PUSH0, RETURN
    };

    address_t from;
    uint64_t gas = 0x134d2;
    val_t value;
    value[0] = value[1] = value[2] = 0;
    data_t input;
    input.size = sizeof(sha3Test);
    input.content = sha3Test;
    result_t result = txCreate(from, gas, value, input);

    // c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a47010ca3eff73ebec87d2394fc58560afeab86dac7a21f5e402ea0a55e5c8a6758f0000000000000000000000000000000000000000000000000000000000000000ec10957fe1361acf6a7655b35eb00faed8b1495e58674196eaa0eb7900b213f2
    assert(result.returnData.size == 128);
    assert(result.returnData.content[0] == 0xc5);
    assert(result.returnData.content[1] == 0xd2);
    assert(result.returnData.content[2] == 0x46);
    assert(result.returnData.content[3] == 0x01);
    assert(result.returnData.content[4] == 0x86);
    assert(result.returnData.content[5] == 0xf7);
    assert(result.returnData.content[6] == 0x23);
    assert(result.returnData.content[7] == 0x3c);
    assert(result.returnData.content[8] == 0x92);
    assert(result.returnData.content[9] == 0x7e);
    assert(result.returnData.content[10] == 0x7d);
    assert(result.returnData.content[11] == 0xb2);
    assert(result.returnData.content[12] == 0xdc);
    assert(result.returnData.content[13] == 0xc7);
    assert(result.returnData.content[14] == 0x03);
    assert(result.returnData.content[15] == 0xc0);
    assert(result.returnData.content[16] == 0xe5);
    assert(result.returnData.content[17] == 0x00);
    assert(result.returnData.content[18] == 0xb6);
    assert(result.returnData.content[19] == 0x53);
    assert(result.returnData.content[20] == 0xca);
    assert(result.returnData.content[21] == 0x82);
    assert(result.returnData.content[22] == 0x27);
    assert(result.returnData.content[23] == 0x3b);
    assert(result.returnData.content[24] == 0x7b);
    assert(result.returnData.content[25] == 0xfa);
    assert(result.returnData.content[26] == 0xd8);
    assert(result.returnData.content[27] == 0x04);
    assert(result.returnData.content[28] == 0x5d);
    assert(result.returnData.content[29] == 0x85);
    assert(result.returnData.content[30] == 0xa4);
    assert(result.returnData.content[31] == 0x70);
    assert(result.returnData.content[32] == 0x10);
    assert(result.returnData.content[33] == 0xca);
    assert(result.returnData.content[34] == 0x3e);
    assert(result.returnData.content[35] == 0xff);
    assert(result.returnData.content[36] == 0x73);
    assert(result.returnData.content[37] == 0xeb);
    assert(result.returnData.content[38] == 0xec);
    assert(result.returnData.content[39] == 0x87);
    assert(result.returnData.content[40] == 0xd2);
    assert(result.returnData.content[41] == 0x39);
    assert(result.returnData.content[42] == 0x4f);
    assert(result.returnData.content[43] == 0xc5);
    assert(result.returnData.content[44] == 0x85);
    assert(result.returnData.content[45] == 0x60);
    assert(result.returnData.content[46] == 0xaf);
    assert(result.returnData.content[47] == 0xea);
    assert(result.returnData.content[48] == 0xb8);
    assert(result.returnData.content[49] == 0x6d);
    assert(result.returnData.content[50] == 0xac);
    assert(result.returnData.content[51] == 0x7a);
    assert(result.returnData.content[52] == 0x21);
    assert(result.returnData.content[53] == 0xf5);
    assert(result.returnData.content[54] == 0xe4);
    assert(result.returnData.content[55] == 0x02);
    assert(result.returnData.content[56] == 0xea);
    assert(result.returnData.content[57] == 0x0a);
    assert(result.returnData.content[58] == 0x55);
    assert(result.returnData.content[59] == 0xe5);
    assert(result.returnData.content[60] == 0xc8);
    assert(result.returnData.content[61] == 0xa6);
    assert(result.returnData.content[62] == 0x75);
    assert(result.returnData.content[63] == 0x8f);
    for (int i = 64; i < 96; i++) {
        assert(result.returnData.content[i] == 0x00);
    }
    assert(result.returnData.content[96] == 0xec);
    assert(result.returnData.content[97] == 0x10);
    assert(result.returnData.content[98] == 0x95);
    assert(result.returnData.content[99] == 0x7f);
    assert(result.returnData.content[100] == 0xe1);
    assert(result.returnData.content[101] == 0x36);
    assert(result.returnData.content[102] == 0x1a);
    assert(result.returnData.content[103] == 0xcf);
    assert(result.returnData.content[104] == 0x6a);
    assert(result.returnData.content[105] == 0x76);
    assert(result.returnData.content[106] == 0x55);
    assert(result.returnData.content[107] == 0xb3);
    assert(result.returnData.content[108] == 0x5e);
    assert(result.returnData.content[109] == 0xb0);
    assert(result.returnData.content[110] == 0x0f);
    assert(result.returnData.content[111] == 0xae);
    assert(result.returnData.content[112] == 0xd8);
    assert(result.returnData.content[113] == 0xb1);
    assert(result.returnData.content[114] == 0x49);
    assert(result.returnData.content[115] == 0x5e);
    assert(result.returnData.content[116] == 0x58);
    assert(result.returnData.content[117] == 0x67);
    assert(result.returnData.content[118] == 0x41);
    assert(result.returnData.content[119] == 0x96);
    assert(result.returnData.content[120] == 0xea);
    assert(result.returnData.content[121] == 0xa0);
    assert(result.returnData.content[122] == 0xeb);
    assert(result.returnData.content[123] == 0x79);
    assert(result.returnData.content[124] == 0x00);
    assert(result.returnData.content[125] == 0xb2);
    assert(result.returnData.content[126] == 0x13);
    assert(result.returnData.content[127] == 0xf2);
    assert(result.gasRemaining == 0);

    evmFinalize();
}

void test_delegateCall() {
    evmInit();

    // 5f365b818111156042577fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff01813560f81c8153803560f81c825390600101906002565b365ff3
#define PROGRAM_REVERSE \
        PUSH0, CALLDATASIZE, \
        JUMPDEST, \
        DUP2, DUP2, GT, ISZERO, PUSH1, 66, JUMPI, \
        PUSH32, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, ADD, \
        DUP2, CALLDATALOAD, PUSH1, 248, SHR, DUP2, MSTORE8, \
        DUP1, CALLDATALOAD, PUSH1, 248, SHR, DUP3, MSTORE8, \
        SWAP1, PUSH1, 1, ADD, SWAP1, \
        PUSH1, 2, JUMP, \
        JUMPDEST, \
        CALLDATASIZE, PUSH0, RETURN
	op_t reverseBytes[] = {
        PROGRAM_REVERSE
	};

    op_t createReverseBytes[] = {
        PUSH1, 70, DUP1, PUSH1, 9, RETURNDATASIZE, CODECOPY, RETURNDATASIZE, RETURN,
        PROGRAM_REVERSE
    };
#undef PROGRAM_REVERSE
    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    uint64_t gas = 0x10ad0;
    val_t value;
    value[0] = value[1] = value[2] = 0;
    data_t input;
    input.size = sizeof(createReverseBytes);
    input.content = createReverseBytes;
    result_t result = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == sizeof(reverseBytes));
    assert(memcmp(result.returnData.content, reverseBytes, sizeof(reverseBytes)) == 0);
    assert(result.gasRemaining == 0);

#define PROGRAM_PROXY \
		CALLDATASIZE, RETURNDATASIZE, RETURNDATASIZE, CALLDATACOPY, \
		RETURNDATASIZE, \
		RETURNDATASIZE, RETURNDATASIZE, CALLDATASIZE, RETURNDATASIZE, PUSH20, ACCOUNT0, GAS, DELEGATECALL, \
		RETURNDATASIZE, DUP3, DUP1, RETURNDATACOPY, \
		SWAP1, RETURNDATASIZE, SWAP2, PUSH1, 43, JUMPI, REVERT, \
		JUMPDEST, RETURN
    op_t delegateCaller[] = {
        PROGRAM_PROXY
    };

    op_t createDelegateCaller[] = {
        PUSH1, 45,
        DUP1, PUSH1, 9, RETURNDATASIZE, CODECOPY, RETURNDATASIZE, RETURN,
        PROGRAM_PROXY
    };
#undef PROGRAM_PROXY

    gas = 0xf5b0;
    input.content = createDelegateCaller;
    input.size = sizeof(createDelegateCaller);

    result_t secondCreateResult = txCreate(from, gas, value, input);
    assert(UPPER(UPPER(secondCreateResult.status)) == 0);
    assert(LOWER(UPPER(secondCreateResult.status)) == 0x47784b21);
    assert(UPPER(LOWER(secondCreateResult.status)) == 0x780d1e1d5efcd005);
    assert(LOWER(LOWER(secondCreateResult.status)) == 0xc5fe542813b6d71e);
    assert(secondCreateResult.returnData.size == sizeof(delegateCaller));
    assert(memcmp(secondCreateResult.returnData.content, delegateCaller, sizeof(delegateCaller)) == 0);
    assert(secondCreateResult.gasRemaining == 0);

    address_t proxy = AddressFromUint256(&secondCreateResult.status);
    gas = 0x6fe8;
    input.content = createReverseBytes;
    input.size = sizeof(createReverseBytes);

    result_t reverseProxy = txCall(from, gas, proxy, value, input, NULL);
    assert(UPPER(UPPER(reverseProxy.status)) == 0);
    assert(LOWER(UPPER(reverseProxy.status)) == 0);
    assert(UPPER(LOWER(reverseProxy.status)) == 0);
    assert(LOWER(LOWER(reverseProxy.status)) == 1);
    assert(reverseProxy.returnData.size == input.size);
    for (size_t i = 0; i < input.size; i++) {
        assert(reverseProxy.returnData.content[i] == input.content[input.size - i - 1]);
    }
    assert(reverseProxy.gasRemaining == 16);

    evmFinalize();
}

int main() {
    test_stop();
    test_mstoreReturn();
    test_math();
    test_shl();
    test_xorSwap();
    test_sgtslt();
    test_exp();
    test_spaghetti();
    test_sstore_sload();
    test_sstore_refund();
    test_sstore_gauntlet();
    test_selfbalance();
    test_callEmpty();
    test_callBounce();
    test_extcodecopy();
    test_deepCall();
    test_revertStorage();
    test_revertSload();
    test_log();
    test_sha3();
    test_delegateCall();

    // These last tests will write to stderr; usually we want this to be hushed
    close(2);

    test_staticcallSstore();

    return 0;
}
