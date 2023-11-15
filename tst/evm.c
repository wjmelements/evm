#include <assert.h>
#include <string.h>
#include <stdio.h>

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

    result_t result = evmCreate(from, gas, value, input);
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

    result_t result = evmCreate(from, gas, value, input);
    evmFinalize();
    // TODO result.status should instead be the return address
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
    uint64_t gas = 53332;
    val_t value;
    data_t input;

    // 0 - (3 * 7 + 4 / 2)
    op_t program[] = {
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
    assert(result.gasRemaining == 0);
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

    result_t result = evmCreate(from, gas, value, input);
    evmFinalize();
    // TODO result.status should instead be the return address
    assert(UPPER(UPPER(result.status)) == 0);
    assert(LOWER(UPPER(result.status)) == 0x80d9b122);
    assert(UPPER(LOWER(result.status)) == 0xdc3a16fdc41f96cf);
    assert(LOWER(LOWER(result.status)) == 0x010ffe7e38d227c3);
    assert(result.returnData.size == 64);
    assert(0 == memcmp(result.returnData.content, program + 1, 32));
    assert(0 == memcmp(result.returnData.content + 32, program + 34, 32));
    assert(result.gasRemaining == 0);
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

    result_t result = evmCreate(from, gas, value, input);
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

    result_t result = evmCreate(from, gas, value, input);
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

    result_t result = evmCreate(from, gas, value, input);
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
    result_t result = evmCreate(from, gas, value, input);
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

    result_t result = evmCall(from, gas, to, value, input);
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

    result_t result = evmCreate(from, gas, value, input);
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

int main() {
    test_stop();
    test_mstoreReturn();
    test_math();
    test_xorSwap();
    test_spaghetti();
    test_sstore_sload();
    test_sstore_refund();
    test_sstore_gauntlet();
    test_selfbalance();
    test_callEmpty();
    test_callBounce();
    return 0;
}
