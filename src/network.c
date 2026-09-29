#include "network.h"
#include "evm.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t rpcId = 0;
static char *rpcBuf;
static size_t rpcCap;
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

static void badResponse(const char *method) {
    fprintf(stderr, "evm: network: bad %s response: %s", method, rpcBuf);
    if (!strchr(rpcBuf, '\n')) {
        fputc('\n', stderr);
    }
    _exit(1);
}

static void readResponse(const char *what) {
    if (getline(&rpcBuf, &rpcCap, stdin) == -1) {
        fprintf(stderr, "evm: network: no response for %s\n", what);
        _exit(1);
    }
}

static uint64_t readResultU64(const char *method) {
    readResponse(method);
    const char *p = rpcBuf;
    if (!nextResultHex(&p)) {
        badResponse(method);
    }
    uint64_t result = 0;
    while (*p != '"' && *p) {
        result = (result << 4) | hexString8ToUint8(*p++);
    }
    return result;
}

static void fetchBlockHeader(block_t *block) {
    printf("{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"eth_getBlockByNumber\",\"params\":[\"0x%" PRIx64 "\",false]}\n", ++rpcId, block->number);
    fflush(stdout);
    readResponse("eth_getBlockByNumber");
    const char *header = jFind(rpcBuf, "result");
    if (!header || *header != '{') {
        badResponse("eth_getBlockByNumber");
    }
    blockFields_t found = 0;
    const char *key, *val;
    size_t klen;
    for (const char *end = header; (end = jNextKeyVal(end, &key, &klen, &val)); ) {
        uint8_t index = blockKeyIndex(blockHeaderKey, key, klen);
        if (index < BLOCK_FIELD_COUNT && *val == '"') {
            blockParseField(block, index, val + 1, end - val - 2);
            found |= (blockFields_t)1 << index;
        }
    }
    if (!(found & BLOCK_BIT(baseFee))) {
        // before London
        clear256(&block->baseFee);
        found |= BLOCK_BIT(baseFee);
    }
    if (found != BLOCK_HEADER) {
        badResponse("eth_getBlockByNumber");
    }
}

static void networkFetchBlock(blockFields_t fields, block_t *block) {
    if (fields == BLOCK_BIT(number)) {
        printf("{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"eth_blockNumber\",\"params\":[]}\n", ++rpcId);
        fflush(stdout);
        block->number = readResultU64("eth_blockNumber");
    } else if (fields == BLOCK_BIT(chainId)) {
        printf("{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"eth_chainId\",\"params\":[]}\n", ++rpcId);
        fflush(stdout);
        block->chainId = readResultU64("eth_chainId");
    } else {
        fetchBlockHeader(block);
    }
}

static void ensureNetworkBlock(void) {
    snprintf(networkBlockHex, sizeof(networkBlockHex), "0x%" PRIx64, evmStateBlockNumber());
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

    readResponse("account fetch");
    const char *p = rpcBuf;

    // code
    const char *hex = nextResultHex(&p);
    if (!hex) {
        badResponse("eth_getCode");
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
        badResponse("eth_getTransactionCount");
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
        badResponse("eth_getBalance");
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

    readResponse("storage fetch");
    const char *p = rpcBuf;
    nextResultHex(&p);
    if (!p) {
        badResponse("eth_getStorageAt");
    }
    clear256(value_out);
    while (*p != '"' && *p) {
        shiftl256(value_out, 4, value_out);
        LOWER(LOWER_P(value_out)) |= hexString8ToUint8(*p++);
    }
}

void evmSetNetworkFetch(void) {
    evmSetFetch(networkFetchAccount, networkFetchStorage);
    evmSetBlockFetch(networkFetchBlock);
}
