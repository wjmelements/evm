#include "network.h"
#include "evm.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t rpcId = 0;
static char rpcBuf[131072];
static char networkBlockHex[20];

// Scan forward in *p to the next "result":"0x<hex>" value.
// Returns a pointer to the first hex character (past "0x").
// Advances *p to the same position; caller scans to '"' for the end.
static const char *nextResultHex(const char **p) {
    *p = strstr(*p, "\"result\"");
    if (!*p) {
        return NULL;
    }
    *p += 8;
    *p = strchr(*p, ':');
    if (!*p) {
        return NULL;
    }
    (*p)++;
    while (**p == ' ') {
        (*p)++;
    }
    if (**p != '"') {
        return NULL;
    }
    (*p)++;
    if ((*p)[0] == '0' && (*p)[1] == 'x') {
        (*p) += 2;
    }
    return *p;
}

static void ensureNetworkBlock(void) {
    if (evmBlockNumberIsSet()) {
        snprintf(networkBlockHex, sizeof(networkBlockHex), "0x%" PRIx64, evmGetBlockNumber());
        return;
    }
    printf("{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"eth_blockNumber\",\"params\":[]}\n", ++rpcId);
    fflush(stdout);
    if (!fgets(rpcBuf, sizeof(rpcBuf), stdin)) {
        fputs("evm: network: no response for eth_blockNumber\n", stderr);
        _exit(1);
    }
    const char *p = rpcBuf;
    const char *hex = nextResultHex(&p);
    if (!hex) {
        fputs("evm: network: bad eth_blockNumber response\n", stderr);
        _exit(1);
    }
    uint64_t block = 0;
    while (*p != '"' && *p) {
        block = (block << 4) | hexString8ToUint8(*p++);
    }
    snprintf(networkBlockHex, sizeof(networkBlockHex), "0x%" PRIx64, block);
    evmSetBlockNumber(block);
}

static void networkFetchAccount(address_t address) {
    ensureNetworkBlock();
    uint32_t base = ++rpcId;
    rpcId += 2;
    putchar('[');
    printf("{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"eth_getCode\",\"params\":[\"", base);
    fprintAddress(stdout, address);
    printf("\",\"%s\"]},", networkBlockHex);
    printf("{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"eth_getTransactionCount\",\"params\":[\"", base + 1);
    fprintAddress(stdout, address);
    printf("\",\"%s\"]},", networkBlockHex);
    printf("{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"eth_getBalance\",\"params\":[\"", base + 2);
    fprintAddress(stdout, address);
    printf("\",\"%s\"]}", networkBlockHex);
    puts("]");
    fflush(stdout);

    if (!fgets(rpcBuf, sizeof(rpcBuf), stdin)) {
        fputs("evm: network: no response for account fetch\n", stderr);
        _exit(1);
    }
    const char *p = rpcBuf;

    // code
    const char *hex = nextResultHex(&p);
    if (!hex) {
        fputs("evm: network: bad eth_getCode response\n", stderr);
        _exit(1);
    }
    const char *codeStart = p;
    while (*p != '"' && *p) {
        p++;
    }
    data_t code;
    code.size = (p - codeStart) / 2;
    code.content = code.size ? malloc(code.size) : NULL;
    for (size_t i = 0; i < code.size; i++) {
        code.content[i] = hexString16ToUint8(codeStart + i * 2);
    }
    evmMockCode(address, code);
    if (*p == '"') {
        p++;
    }

    // nonce
    hex = nextResultHex(&p);
    if (!hex) {
        fputs("evm: network: bad eth_getTransactionCount response\n", stderr);
        _exit(1);
    }
    uint64_t nonce = 0;
    while (*p != '"' && *p) {
        nonce = (nonce << 4) | hexString8ToUint8(*p++);
    }
    evmMockNonce(address, nonce);
    if (*p == '"') {
        p++;
    }

    // balance
    hex = nextResultHex(&p);
    if (!hex) {
        fputs("evm: network: bad eth_getBalance response\n", stderr);
        _exit(1);
    }
    val_t balance = {0, 0, 0};
    while (*p != '"' && *p) {
        balance[0] = (balance[0] << 4) | (balance[1] >> 28);
        balance[1] = (balance[1] << 4) | (balance[2] >> 28);
        balance[2] = (balance[2] << 4) | hexString8ToUint8(*p++);
    }
    evmMockBalance(address, balance);
}

static void networkFetchStorage(address_t address, const uint256_t *key, uint256_t *value_out) {
    ensureNetworkBlock();
    printf("{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"eth_getStorageAt\",\"params\":[\"", ++rpcId);
    fprintAddress(stdout, address);
    fputs("\",\"0x", stdout);
    fprint256(stdout, key);
    printf("\",\"%s\"]}\n", networkBlockHex);
    fflush(stdout);

    if (!fgets(rpcBuf, sizeof(rpcBuf), stdin)) {
        fputs("evm: network: no response for storage fetch\n", stderr);
        _exit(1);
    }
    const char *p = rpcBuf;
    nextResultHex(&p);
    if (!p) {
        fputs("evm: network: bad eth_getStorageAt response\n", stderr);
        _exit(1);
    }
    clear256(value_out);
    while (*p != '"' && *p) {
        shiftl256(value_out, 4, value_out);
        LOWER(LOWER_P(value_out)) |= hexString8ToUint8(*p++);
    }
}

void evmSetNetworkFetch(void) {
    evmSetFetch(networkFetchAccount, networkFetchStorage);
}
