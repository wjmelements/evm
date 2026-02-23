#include "dio.h"
#include "path.h"


#include <assert.h>
#include <unistd.h>

void test_applyConfig_code() {
    evmInit();
    const char config[] = "["
        "{"
            "\"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
            "\"code\":\"0x383d3d39383df3\""
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

    result_t result = txCall(from, gas, to, val, input, NULL);
    op_t expected[] = {
         CODESIZE, RETURNDATASIZE, RETURNDATASIZE, CODECOPY,
         CODESIZE, RETURNDATASIZE, RETURN
    };
    assert(result.returnData.size == sizeof(expected));
    assert(memcmp(expected, result.returnData.content, result.returnData.size) == 0);
    assert(result.gasRemaining == 0);

    evmFinalize();
}

void test_applyConfig_storage() {
    evmInit();

    const char config[] = "["
        "{"
            "\"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
            "\"code\":\"0x5f545f52595ff3\","
            "\"storage\":{"
                "\"0x00\": \"0x12340000567800009abc0000def0\""
            "}"
        "}"
    "]";
    applyConfig(config);

    uint64_t gas = 23114;
    address_t from;
    val_t val;
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    address_t to = AddressFromHex42("0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3");
    data_t input;
    input.size = 0;

    result_t result = txCall(from, gas, to, val, input, NULL);
    op_t expected[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x12, 0x34, 0x00, 0x00, 0x56, 0x78,
        0x00, 0x00, 0x9a, 0xbc, 0x00, 0x00, 0xde, 0xf0,
    };
    assert(result.returnData.size == sizeof(expected));
    assert(memcmp(expected, result.returnData.content, result.returnData.size) == 0);
    assert(result.gasRemaining == 0);

    evmFinalize();
}


void test_applyConfig_balance() {
    evmInit();

    const char config[] = "["
        "{"
            "\"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
            "\"code\":\"0x475f52595ff3\","
            "\"balance\":\"0x1234005678009abc00def0\""
        "}"
    "]";
    applyConfig(config);

    uint64_t gas = 21017;
    address_t from;
    val_t val;
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    address_t to = AddressFromHex42("0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3");
    data_t input;
    input.size = 0;

    result_t result = txCall(from, gas, to, val, input, NULL);
    op_t expected[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x34, 0x00,
        0x56, 0x78, 0x00, 0x9a, 0xbc, 0x00, 0xde, 0xf0,
    };
    assert(result.returnData.size == sizeof(expected));
    assert(memcmp(expected, result.returnData.content, result.returnData.size) == 0);
    assert(result.gasRemaining == 0);

    evmFinalize();
}

void test_applyConfig_construct() {
    evmInit();

    const char config[] = "["
        "{"
            "\"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
            "\"construct\":\"tst/in/quine.evm\","
            "\"code\":\"0x383d3d39383df3\""
        "}"
    "]";
    applyConfig(config);

    uint64_t gas = 0x521b;
    address_t from;
    val_t val;
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    address_t to = AddressFromHex42("0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3");
    data_t input;
    input.size = 0;

    result_t result = txCall(from, gas, to, val, input, NULL);
    op_t expected[] = {
        0x38, 0x3d, 0x3d, 0x39, 0x38, 0x3d, 0xf3
    };
    assert(result.returnData.size == sizeof(expected));
    assert(memcmp(expected, result.returnData.content, result.returnData.size) == 0);
    assert(result.gasRemaining == 0);

    evmFinalize();
}


void test_applyConfig_constructTest() {
    evmInit();

    const char config[] = "["
        "{"
            "\"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
            "\"construct\":\"tst/in/quine.evm\","
            "\"code\":\"0x383d3d39383df3\","
            "\"constructTest\":{}"
        "}"
    "]";
    applyConfig(config);

    uint64_t gas = 0x521b;
    address_t from;
    val_t val;
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    address_t to = AddressFromHex42("0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3");
    data_t input;
    input.size = 0;

    result_t result = txCall(from, gas, to, val, input, NULL);
    op_t expected[] = {
        0x38, 0x3d, 0x3d, 0x39, 0x38, 0x3d, 0xf3
    };
    assert(result.returnData.size == sizeof(expected));
    assert(memcmp(expected, result.returnData.content, result.returnData.size) == 0);
    assert(result.gasRemaining == 0);

    evmFinalize();
}

void test_applyConfig_tests() {
    evmInit();

    const char config[] = "["
        "{"
            "\"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
            "\"code\":\"0x60213610600e576020355f3555005b5f35545f52595ff3\","
            "\"tests\":"
                "["
                    "{"
                        "\"op\": \"STATICCALL\","
                        "\"input\": \"0x0000000000000000000000000000000000000000000000000000000000000000\","
                        "\"output\": \"0x0000000000000000000000000000000000000000000000000000000000000000\""
                    "},"
                    "{"
                        "\"op\": \"CALL\","
                        "\"input\": \"0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001\","
                        "\"output\": \"0x\""
                    "},"
                    "{"
                        "\"op\": \"STATICCALL\","
                        "\"input\": \"0x0000000000000000000000000000000000000000000000000000000000000000\","
                        "\"output\": \"0x0000000000000000000000000000000000000000000000000000000000000001\""
                    "}"
                "]"
        "}"
    "]";
    applyConfig(config);

    uint64_t gas = 0x5a63;
    address_t from;
    val_t val;
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    address_t to = AddressFromHex42("0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3");
    data_t input;
    input.size = 0;

    result_t result = txCall(from, gas, to, val, input, NULL);
    op_t expected[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    };
    assert(result.returnData.size == sizeof(expected));
    assert(memcmp(expected, result.returnData.content, result.returnData.size) == 0);
    assert(result.gasRemaining == 0);

    evmFinalize();
}

int main() {
    pathInit("bin/evm");

    test_applyConfig_code();
    test_applyConfig_storage();
    test_applyConfig_balance();
    test_applyConfig_construct();
    test_applyConfig_constructTest();

    close(2);
    test_applyConfig_tests();
    return 0;
}
