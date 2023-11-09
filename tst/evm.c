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

void test_mstore_return() {
    evmInit();

    address_t from;
    uint64_t gas;
    val_t value;
    data_t input;

    uint8_t program[] = {
        PUSH32, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
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

int main() {
    test_stop();
    test_mstore_return();
    return 0;
}
