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

    result_t result = txCreate(from, gas, value, input);
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

    result_t result = txCall(from, gas, to, value, input);
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
    result_t examineFirstAccount = txCall(from, gas, to, value, input);
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
    result_t examineSecondAccount = txCall(from, gas, to, value, input);
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
    result_t diveResult = txCall(from, gas, to, value, input);

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

    result_t sstoreRevertResult = txCall(from, gas, to, value, input);
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
    test_extcodecopy();
    test_deepCall();
    test_revertStorage();
    return 0;
}
