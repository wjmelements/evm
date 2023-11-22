#include "dio.h"

#include <assert.h>

void test_applyConfig_code() {
    evmInit();
#define PROGRAM_QUINE "383d3d39383df3"
    const char config[] = "["
        "{"
            "\"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
            "\"code\":\"0x" PROGRAM_QUINE "\""
        "}"
    "]";
    applyConfig(config);

    uint64_t gas = 21019;
    address_t from;
    val_t val;
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    address_t to = AddressFromHex42("0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3");
    data_t input;
    input.size = 0;

    result_t result = txCall(from, gas, to, val, input);
    op_t expected[] = {
         CODESIZE, RETURNDATASIZE, RETURNDATASIZE, CODECOPY,
         CODESIZE, RETURNDATASIZE, RETURN
    };
    assert(result.returnData.size == sizeof(expected));
    assert(memcmp(expected, result.returnData.content, result.returnData.size) == 0);
    assert(result.gasRemaining == 0);

    evmFinalize();
}

int main() {
    test_applyConfig_code();
    // TODO test_applyConfig_storage();
    return 0;
}
