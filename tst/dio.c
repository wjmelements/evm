#include "dio.h"
#include "path.h"


#include <assert.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

void test_applyConfig_code() {
    evmInit();
    const char config[] =
        "["
        "    {"
        "        \"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
        "        \"code\":\"0x383d3d39383df3\""
        "    }"
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

    const char config[] =
        "["
        "    {"
        "        \"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
        "        \"code\":\"0x5f545f52595ff3\","
        "        \"storage\":{"
        "            \"0x00\": \"0x12340000567800009abc0000def0\""
        "        }"
        "    }"
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

    const char config[] =
        "["
        "    {"
        "        \"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
        "        \"code\":\"0x475f52595ff3\","
        "        \"balance\":\"0x1234005678009abc00def0\""
        "    }"
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

    const char config[] =
        "["
        "    {"
        "        \"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
        "        \"construct\":\"tst/in/quine.evm\","
        "        \"code\":\"0x383d3d39383df3\""
        "    }"
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

    const char config[] =
        "["
        "    {"
        "        \"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
        "        \"construct\":\"tst/in/quine.evm\","
        "        \"code\":\"0x383d3d39383df3\","
        "        \"constructTest\":{}"
        "    }"
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

    const char config[] =
        "["
        "    {"
        "        \"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
        "        \"code\":\"0x60213610600e576020355f3555005b5f35545f52595ff3\","
        "        \"tests\":"
        "            ["
        "                {"
        "                    \"op\": \"STATICCALL\","
        "                    \"input\": \"0x0000000000000000000000000000000000000000000000000000000000000000\","
        "                    \"output\": \"0x0000000000000000000000000000000000000000000000000000000000000000\""
        "                },"
        "                {"
        "                    \"op\": \"CALL\","
        "                    \"input\": \"0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001\","
        "                    \"output\": \"0x\""
        "                },"
        "                {"
        "                    \"op\": \"STATICCALL\","
        "                    \"input\": \"0x0000000000000000000000000000000000000000000000000000000000000000\","
        "                    \"output\": \"0x0000000000000000000000000000000000000000000000000000000000000001\""
        "                }"
        "            ]"
        "    }"
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

// A short "from" address is left-padded with zeros.
void test_applyConfig_from_short() {
    evmInit();

    const char config[] =
        "["
        "    {"
        "        \"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
        "        \"code\":\"0x335f5260205ff3\","
        "        \"tests\":"
        "            ["
        "                {"
        "                    \"op\": \"CALL\","
        "                    \"from\": \"0x1234\","
        "                    \"input\": \"0x\","
        "                    \"output\": \"0x0000000000000000000000000000000000000000000000000000000000001234\""
        "                }"
        "            ]"
        "    }"
        "]";
    // applyConfig calls _exit(1) if the test's expected output does not match.
    applyConfig(config);

    evmFinalize();
}

// A "from" address with more than 40 hex digits is rejected.
void test_applyConfig_from_long() {
    const char config[] =
        "["
        "    {"
        "        \"address\":\"0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3\","
        "        \"code\":\"0x335f5260205ff3\","
        "        \"tests\":"
        "            ["
        "                {"
        "                    \"op\": \"CALL\","
        "                    \"from\": \"0xaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\","
        "                    \"input\": \"0x\","
        "                    \"output\": \"0x\""
        "                }"
        "            ]"
        "    }"
        "]";
    const char *expectedErr = "Config: address too long (42) on line 1\n";

    int rw[2];
    assert(pipe(rw) == 0);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        dup2(rw[1], 2);
        clearerr(stderr);  // main() closed fd 2; drop the stale error flag
        close(rw[0]);
        close(rw[1]);
        evmInit();
        applyConfig(config);
        _exit(0);
    }
    close(rw[1]);
    char actualErr[128];
    ssize_t red = read(rw[0], actualErr, sizeof(actualErr) - 1);
    assert(red >= 0);
    actualErr[red] = '\0';
    close(rw[0]);

    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    assert(strcmp(actualErr, expectedErr) == 0);
}

int main() {
    pathInit("bin/evm");

    test_applyConfig_code();
    test_applyConfig_storage();
    test_applyConfig_balance();
    test_applyConfig_construct();

    close(2);
    test_applyConfig_constructTest();
    test_applyConfig_tests();
    test_applyConfig_from_short();
    test_applyConfig_from_long();
    return 0;
}
