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
    // applyConfig calls exit(1) if the test's expected output does not match.
    applyConfig(config);

    evmFinalize();
}

// Run fn(arg) in a child, asserting that it exits 1 with exactly expectedErr on stderr.
static void assertFails(void (*fn)(const char *), const char *arg, const char *expectedErr) {
    int rw[2];
    assert(pipe(rw) == 0);
    fflush(stderr);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        dup2(rw[1], 2);
        close(rw[0]);
        close(rw[1]);
        evmInit();
        fn(arg);
        _exit(0);
    }
    close(rw[1]);
    char actualErr[512];
    size_t total = 0;
    ssize_t red;
    while ((red = read(rw[0], actualErr + total, sizeof(actualErr) - 1 - total)) > 0) {
        total += red;
    }
    assert(red == 0);
    actualErr[total] = '\0';
    close(rw[0]);

    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    assert(strcmp(actualErr, expectedErr) == 0);
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
    assertFails(applyConfig, config, "Config: address too long (42) on line 1\n");
}

// Truncated configs report the end of input instead of reading past it.
void test_applyConfig_truncated() {
    assertFails(applyConfig, "", "Config: expecting '[', found unexpected end of file on line 1\n");
    assertFails(applyConfig, "[", "Config: expecting '{', found unexpected end of file on line 1\n");
    assertFails(applyConfig, "[{\"balance\":\"0x12", "Config: expecting '\"', found unexpected end of file on line 1\n");
    assertFails(applyConfig, "[{\"nonce\":\"0x12", "Config: expecting '\"', found unexpected end of file on line 1\n");
    assertFails(applyConfig, "[{\"storage\":{\"0x01", "Config: expecting '\"', found unexpected end of file on line 1\n");
    assertFails(applyConfig, "[{\"storage\":{\"0x01\":\"0x02", "Config: expecting '\"', found unexpected end of file on line 1\n");
    assertFails(applyConfig, "[{\"address\":\"0x1234", "Config: expecting '\"', found unexpected end of file on line 1\n");
    assertFails(applyConfig, "[{\"code\":\"0x00\",\"tests\":[{\"input\":\"0x", "Config: expecting '\"', found unexpected end of file on line 1\n");
    assertFails(applyConfig, "[{\"code\":\"0x00\",\"tests\":[{\"input\":\"0x\"\n\n", "Config: expecting '}', found unexpected end of file on line 3\n");
    assertFails(applyConfig, "[{\"code\":\"0x00\",\"foo\":{\"a\":[1", "Unexpected entry heading \"foo\" on line 1\nConfig: expecting ']', found unexpected end of file on line 1\n");
    assertFails(applyConfig, "[{\"code\":\"0x00\",\"foo\":\"ab", "Unexpected entry heading \"foo\" on line 1\nConfig: expecting '\"', found unexpected end of file on line 1\n");
}

static void loadConfigNoUpdate(const char *path) {
    loadConfig(path, false);
}

// A truncated config file filling exactly one page is not followed by mapped zero padding.
void test_loadConfig_truncated_page() {
    char path[] = "/tmp/dio_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd != -1);
    const char prefix[] = "[{\"code\":\"0x00\",\"tests\":[{\"input\":\"0x\"";
    size_t pageSize = getpagesize();
    char *contents = malloc(pageSize);
    memset(contents, ' ', pageSize);
    memcpy(contents, prefix, sizeof(prefix) - 1);
    assert(write(fd, contents, pageSize) == (ssize_t)pageSize);
    close(fd);
    free(contents);

    assertFails(loadConfigNoUpdate, path, "Config: expecting '}', found unexpected end of file on line 1\n");

    unlink(path);
}

int main() {
    pathInit("bin/evm");

    test_applyConfig_code();
    test_applyConfig_storage();
    test_applyConfig_balance();
    test_applyConfig_construct();

    freopen("/dev/null", "w", stderr);
    test_applyConfig_constructTest();
    test_applyConfig_tests();
    test_applyConfig_from_short();
    test_applyConfig_from_long();
    test_applyConfig_truncated();
    test_loadConfig_truncated_page();
    return 0;
}
