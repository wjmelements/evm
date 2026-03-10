#include "config.h"
#include "tst.h"

#include <assert.h>

/* Read an entire file into a malloc'd string. Caller must free(). */
static char *readFile(const char *path) {
    FILE *f = fopen(path, "r");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/* Write to a temp file, assert stderr, read contents, unlink, return contents. */
static char *captureWriteConfig(
    account_t     *accounts,
    call_result_t *creates,
    call_result_t *calls)
{
    char path[] = "/tmp/config_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd != -1);
    close(fd);

    char expectedStderr[64];
    snprintf(expectedStderr, sizeof(expectedStderr), "Wrote %s\n", path);
    assertStderr(expectedStderr, writeConfig(accounts, creates, calls, path));

    char *out = readFile(path);
    unlink(path);
    return out;
}

/* ------------------------------------------------------------------ */

void test_single_account_defaults() {
    /* balance "0x0", nonce "0x0", code "0x" — all omitted */
    account_t a = {
        .address = "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = NULL,
        .next    = NULL,
    };
    char *out = captureWriteConfig(&a, NULL, NULL);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\""
        "\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_account_with_balance() {
    account_t a = {
        .address = "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        .balance = "0x1",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = NULL,
        .next    = NULL,
    };
    char *out = captureWriteConfig(&a, NULL, NULL);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\","
        "\n        \"balance\": \"0x1\""
        "\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_account_with_nonce() {
    account_t a = {
        .address = "0xcccccccccccccccccccccccccccccccccccccccc",
        .balance = "0x0",
        .nonce   = "0x5",
        .code    = "0x",
        .storage = NULL,
        .next    = NULL,
    };
    char *out = captureWriteConfig(&a, NULL, NULL);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xcccccccccccccccccccccccccccccccccccccccc\","
        "\n        \"nonce\": \"0x5\""
        "\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_account_with_code() {
    account_t a = {
        .address = "0xdddddddddddddddddddddddddddddddddddddddd",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x6001",
        .storage = NULL,
        .next    = NULL,
    };
    char *out = captureWriteConfig(&a, NULL, NULL);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xdddddddddddddddddddddddddddddddddddddddd\","
        "\n        \"code\": \"0x6001\""
        "\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_account_with_storage() {
    storage_kv_t s = { .key = "0x1", .value = "0xff", .next = NULL };
    account_t a = {
        .address = "0xeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = &s,
        .next    = NULL,
    };
    char *out = captureWriteConfig(&a, NULL, NULL);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\","
        "\n        \"storage\": {\n"
        "            \"0x1\": \"0xff\""
        "\n        }"
        "\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_account_with_two_storage_slots() {
    /* next pointer links slots; first in list prints first */
    storage_kv_t s2 = { .key = "0x2", .value = "0x20", .next = NULL };
    storage_kv_t s1 = { .key = "0x1", .value = "0x10", .next = &s2 };
    account_t a = {
        .address = "0xffffffffffffffffffffffffffffffffffffffff",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = &s1,
        .next    = NULL,
    };
    char *out = captureWriteConfig(&a, NULL, NULL);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xffffffffffffffffffffffffffffffffffffffff\","
        "\n        \"storage\": {\n"
        "            \"0x1\": \"0x10\","
        "\n            \"0x2\": \"0x20\""
        "\n        }"
        "\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_two_accounts() {
    account_t a2 = {
        .address = "0x2222222222222222222222222222222222222222",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = NULL,
        .next    = NULL,
    };
    account_t a1 = {
        .address = "0x1111111111111111111111111111111111111111",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = NULL,
        .next    = &a2,
    };
    char *out = captureWriteConfig(&a1, NULL, NULL);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0x1111111111111111111111111111111111111111\""
        "\n    },"
        "\n    {\n"
        "        \"address\": \"0x2222222222222222222222222222222222222222\""
        "\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_create_entry() {
    /* status is the deployed address (success) — omitted from output */
    account_t acct = {
        .address = "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = NULL,
        .next    = NULL,
    };
    call_result_t cr = {
        .to      = "",
        .from    = "0x0000000000000000000000000000000000000000",
        .block   = "latest",
        .value   = "",
        .input   = "0x6001",
        .output  = NULL,
        .logs    = NULL,
        .status  = "0xbd770416a3345f91e4b34576cb804a576fa48eb1",
        .gasUsed = NULL,
        .next    = NULL,
    };
    char *out = captureWriteConfig(&acct, &cr, NULL);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\""
        "\n    },"
        "\n    {\n"
        "        \"initcode\": \"0x6001\","
        "\n        \"constructTest\": {"
        "\n            \"blockNumber\": \"latest\""
        "\n        }\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_create_failed() {
    /* status "0x0" means revert — written explicitly */
    account_t acct = {
        .address = "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = NULL,
        .next    = NULL,
    };
    call_result_t cr = {
        .to      = "",
        .from    = "0xdeaddeaddeaddeaddeaddeaddeaddeaddeaddead",
        .block   = "0x1",
        .value   = "",
        .input   = "0x",
        .output  = NULL,
        .logs    = NULL,
        .status  = "0x0",
        .gasUsed = "0x5208",
        .next    = NULL,
    };
    char *out = captureWriteConfig(&acct, &cr, NULL);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\""
        "\n    },"
        "\n    {\n"
        "        \"initcode\": \"0x\","
        "\n        \"constructTest\": {"
        "\n            \"from\": \"0xdeaddeaddeaddeaddeaddeaddeaddeaddeaddead\","
        "\n            \"blockNumber\": \"0x1\","
        "\n            \"gasUsed\": \"0x5208\","
        "\n            \"status\": \"0x0\""
        "\n        }\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_call_entry() {
    account_t acct = {
        .address = "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = NULL,
        .next    = NULL,
    };
    call_result_t call = {
        .to      = "0x1234567890123456789012345678901234567890",
        .from    = "0x0000000000000000000000000000000000000000",
        .block   = "latest",
        .value   = "",
        .input   = "0x",
        .output  = NULL,
        .logs    = NULL,
        .status  = "0x1",
        .gasUsed = NULL,
        .next    = NULL,
    };
    char *out = captureWriteConfig(&acct, NULL, &call);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\""
        "\n    },"
        "\n    {\n        \"tests\": [\n"
        "            {\n"
        "                \"to\": \"0x1234567890123456789012345678901234567890\","
        "\n                \"blockNumber\": \"latest\""
        "\n            }"
        "\n        ]\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_call_with_from_input_gasused_status_output() {
    account_t acct = {
        .address = "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = NULL,
        .next    = NULL,
    };
    call_result_t call = {
        .to      = "0x1234567890123456789012345678901234567890",
        .from    = "0xdeaddeaddeaddeaddeaddeaddeaddeaddeaddead",
        .block   = "0xa",
        .value   = "",
        .input   = "0xdeadbeef",
        .output  = "0xcafe",
        .logs    = NULL,
        .status  = "0x1",
        .gasUsed = "0x5208",
        .next    = NULL,
    };
    char *out = captureWriteConfig(&acct, NULL, &call);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\""
        "\n    },"
        "\n    {\n        \"tests\": [\n"
        "            {\n"
        "                \"to\": \"0x1234567890123456789012345678901234567890\","
        "\n                \"from\": \"0xdeaddeaddeaddeaddeaddeaddeaddeaddeaddead\","
        "\n                \"input\": \"0xdeadbeef\","
        "\n                \"blockNumber\": \"0xa\","
        "\n                \"gasUsed\": \"0x5208\","
        "\n                \"output\": \"0xcafe\""
        "\n            }"
        "\n        ]\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

void test_two_calls() {
    account_t acct = {
        .address = "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        .balance = "0x0",
        .nonce   = "0x0",
        .code    = "0x",
        .storage = NULL,
        .next    = NULL,
    };
    call_result_t call2 = {
        .to      = "0x2222222222222222222222222222222222222222",
        .from    = "0x0000000000000000000000000000000000000000",
        .block   = "latest",
        .value   = "",
        .input   = "0x",
        .output  = NULL,
        .logs    = NULL,
        .status  = "0x1",
        .gasUsed = NULL,
        .next    = NULL,
    };
    call_result_t call1 = {
        .to      = "0x1111111111111111111111111111111111111111",
        .from    = "0x0000000000000000000000000000000000000000",
        .block   = "latest",
        .value   = "",
        .input   = "0x",
        .output  = NULL,
        .logs    = NULL,
        .status  = "0x1",
        .gasUsed = NULL,
        .next    = &call2,
    };
    char *out = captureWriteConfig(&acct, NULL, &call1);
    const char *expected =
        "[\n"
        "    {\n"
        "        \"address\": \"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\""
        "\n    },"
        "\n    {\n        \"tests\": [\n"
        "            {\n"
        "                \"to\": \"0x1111111111111111111111111111111111111111\","
        "\n                \"blockNumber\": \"latest\""
        "\n            },"
        "\n            {\n"
        "                \"to\": \"0x2222222222222222222222222222222222222222\","
        "\n                \"blockNumber\": \"latest\""
        "\n            }"
        "\n        ]\n    }"
        "\n]\n";
    assert(strcmp(out, expected) == 0);
    free(out);
}

int main() {
    test_single_account_defaults();
    test_account_with_balance();
    test_account_with_nonce();
    test_account_with_code();
    test_account_with_storage();
    test_account_with_two_storage_slots();
    test_two_accounts();
    test_create_entry();
    test_create_failed();
    test_call_entry();
    test_call_with_from_input_gasused_status_output();
    test_two_calls();
    return 0;
}
