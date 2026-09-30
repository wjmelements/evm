#include "evm.h"
#include "network.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Fork, wire pipes onto child stdin/stdout/stderr, run child() in child and
// parent(req,rsp) in parent, then assert child exited with expectedStatus
// and its captured stderr matched expectedStderr exactly.
static void with_mock_rpc(void (*child)(void), void (*parent)(FILE *req, FILE *rsp), int expectedStatus, const char *expectedStderr) {
    int req_pipe[2], rsp_pipe[2], err_pipe[2];
    assert(pipe(req_pipe) == 0);
    assert(pipe(rsp_pipe) == 0);
    assert(pipe(err_pipe) == 0);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        close(req_pipe[0]);
        close(rsp_pipe[1]);
        close(err_pipe[0]);
        dup2(rsp_pipe[0], STDIN_FILENO);
        dup2(req_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(rsp_pipe[0]);
        close(req_pipe[1]);
        close(err_pipe[1]);
        child();
        exit(0);
    }
    close(req_pipe[1]);
    close(rsp_pipe[0]);
    close(err_pipe[1]);
    FILE *req = fdopen(req_pipe[0], "r");
    FILE *rsp = fdopen(rsp_pipe[1], "w");
    parent(req, rsp);
    fclose(rsp);
    fclose(req);
    int status;
    waitpid(pid, &status, 0);

    char errBuf[4096];
    ssize_t errLen = read(err_pipe[0], errBuf, sizeof(errBuf) - 1);
    assert(errLen >= 0);
    errBuf[errLen] = 0;
    close(err_pipe[0]);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != expectedStatus || strcmp(errBuf, expectedStderr)) {
        fputs(errBuf, stderr);
    }
    assert(WIFEXITED(status) && WEXITSTATUS(status) == expectedStatus);
    assert(strcmp(errBuf, expectedStderr) == 0);
}

static void batch_response(FILE *rsp, const char *code_hex, const char *balance_hex) {
    fprintf(rsp,
            "[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"%s\"},"
            "{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":\"0x0\"},"
            "{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":\"%s\"}]\n",
            code_hex, balance_hex);
    fflush(rsp);
}

// --- test_networkFetchStorage ---
// Contract: PUSH0 SLOAD PUSH0 MSTORE MSIZE PUSH0 RETURN
// Loads slot 0 from network and returns it as 32 bytes.
static void child_storage(void) {
    evmSetNetworkFetch();
    evmInit();
    evmSetBlockNumber(0x100);

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    address_t addr = AddressFromHex42("0x1111000000000000000000000000000000000001");
    val_t val;
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    data_t input;
    input.size = 0;
    result_t result = txCall(from, 0x5a4a, addr, val, input, NULL);

    assert(result.returnData.size == 32);
    assert(result.gasRemaining == 0);
    for (int i = 0; i < 28; i++) {
        assert(result.returnData.content[i] == 0);
    }
    assert(result.returnData.content[28] == 0x12);
    assert(result.returnData.content[29] == 0x34);
    assert(result.returnData.content[30] == 0x56);
    assert(result.returnData.content[31] == 0x78);

    evmFinalize();
}

static void parent_storage(FILE *req, FILE *rsp) {
    char buf[8192];
    while (fgets(buf, sizeof(buf), req)) {
        if (strstr(buf, "getStorageAt")) {
            assert(strstr(buf, "0x1111000000000000000000000000000000000001") != NULL);
            assert(strstr(buf, "0x0000000000000000000000000000000000000000000000000000000000000000") != NULL);
            fputs("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x12345678\"}\n", rsp);
            fflush(rsp);
        } else if (strstr(buf, "0x1111000000000000000000000000000000000001")) {
            batch_response(rsp, "0x5f545f52595ff3", "0x0");
        } else {
            batch_response(rsp, "0x", "0x0");
        }
    }
}

void test_networkFetchStorage(void) {
    with_mock_rpc(child_storage, parent_storage, 0, "");
}

// --- test_networkFetchStorageRpcError ---
// A JSON-RPC error response (no "result" key) for eth_getStorageAt must
// make the client exit rather than silently misparse a garbage value.
static void parent_storage_rpc_error(FILE *req, FILE *rsp) {
    char buf[8192];
    while (fgets(buf, sizeof(buf), req)) {
        if (strstr(buf, "getStorageAt")) {
            fputs("{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32000,\"message\":\"missing trie node\"}}\n", rsp);
            fflush(rsp);
        } else if (strstr(buf, "0x1111000000000000000000000000000000000001")) {
            batch_response(rsp, "0x5f545f52595ff3", "0x0");
        } else {
            batch_response(rsp, "0x", "0x0");
        }
    }
}

void test_networkFetchStorageRpcError(void) {
    with_mock_rpc(child_storage, parent_storage_rpc_error, 1, "evm: network: bad eth_getStorageAt response: {\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32000,\"message\":\"missing trie node\"}}\n");
}

// --- test_networkFetchAccount ---
// Contract: PUSH20 <target> BALANCE PUSH0 MSTORE MSIZE PUSH0 RETURN
// target = 0x2222000000000000000000000000000000000002 (fetched from network)
static void child_account(void) {
    evmSetNetworkFetch();
    evmInit();
    evmSetBlockNumber(0x100);

    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    address_t addr = AddressFromHex42("0x1111000000000000000000000000000000000001");
    val_t val;
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    data_t input;
    input.size = 0;
    result_t result = txCall(from, 0x5c3f, addr, val, input, NULL);

    assert(result.returnData.size == 32);
    assert(result.gasRemaining == 0);
    for (int i = 0; i < 28; i++) {
        assert(result.returnData.content[i] == 0);
    }
    assert(result.returnData.content[28] == 0xde);
    assert(result.returnData.content[29] == 0xad);
    assert(result.returnData.content[30] == 0xbe);
    assert(result.returnData.content[31] == 0xef);

    evmFinalize();
}

static void parent_account(FILE *req, FILE *rsp) {
    // PUSH20 <target> BALANCE PUSH0 MSTORE MSIZE PUSH0 RETURN
    static const char *addr_code =
        "0x732222000000000000000000000000000000000002315f52595ff3";
    char buf[8192];
    bool seen_contract = false, seen_target = false;
    while (fgets(buf, sizeof(buf), req)) {
        if (strstr(buf, "0x1111000000000000000000000000000000000001")) {
            seen_contract = true;
            batch_response(rsp, addr_code, "0x0");
        } else if (strstr(buf, "0x2222000000000000000000000000000000000002")) {
            seen_target = true;
            batch_response(rsp, "0x", "0xdeadbeef");
        } else {
            batch_response(rsp, "0x", "0x0");
        }
    }
    assert(seen_contract);
    assert(seen_target);
}

void test_networkFetchAccount(void) {
    with_mock_rpc(child_account, parent_account, 0, "");
}

// --- block fetches ---
// The parent serves contract code, eth_chainId, eth_blockNumber, and a block header, counting requests.
static const char *blockContractCode;
static const char *expectedStateTag;
static const char *expectedHeaderNumber;
static int chainIdRequests;
static int blockNumberRequests;
static int headerRequests;
static int minerRequests;
static bool headerMissing;

static void parent_block(FILE *req, FILE *rsp) {
    char buf[8192];
    chainIdRequests = blockNumberRequests = headerRequests = minerRequests = 0;
    while (fgets(buf, sizeof(buf), req)) {
        const char *id = strstr(buf, "\"id\":");
        assert(id != NULL);
        int requestId = atoi(id + 5);
        if (strstr(buf, "eth_chainId")) {
            chainIdRequests++;
            fprintf(rsp, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":\"0x13a\"}\n", requestId);
        } else if (strstr(buf, "eth_blockNumber")) {
            blockNumberRequests++;
            fprintf(rsp, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":\"0x100\"}\n", requestId);
        } else if (strstr(buf, "eth_getBlockByNumber")) {
            headerRequests++;
            char quoted[24];
            snprintf(quoted, sizeof(quoted), "\"%s\"", expectedHeaderNumber);
            assert(strstr(buf, quoted) != NULL);
            if (headerMissing) {
                fprintf(rsp, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}\n", requestId);
            } else {
                fprintf(rsp,
                        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"number\":\"%s\",\"hash\":\"0xabcd\","
                        "\"miner\":\"0x2222222222222222222222222222222222222222\",\"timestamp\":\"0x6700\","
                        "\"gasLimit\":\"0x2faf080\",\"baseFeePerGas\":\"0x3b9aca00\","
                        "\"mixHash\":\"0x0000000000000000000000000000000000000000000000000000000000001234\","
                        "\"transactions\":[\"0x01\",\"0x02\"]}}\n",
                        requestId, expectedHeaderNumber);
            }
        } else {
            assert(strstr(buf, expectedStateTag) != NULL);
            if (strstr(buf, "0x2222222222222222222222222222222222222222")) {
                minerRequests++;
            }
            if (strstr(buf, "0x1111000000000000000000000000000000000001")) {
                batch_response(rsp, blockContractCode, "0x0");
            } else {
                batch_response(rsp, "0x", "0x0");
            }
        }
        fflush(rsp);
    }
}

static result_t callBlockContract(void) {
    address_t from = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    address_t addr = AddressFromHex42("0x1111000000000000000000000000000000000001");
    val_t val;
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    data_t input;
    input.size = 0;
    return txCall(from, 100000, addr, val, input, NULL);
}

static uint64_t returnWord(const result_t *result, int word) {
    uint64_t value = 0;
    for (int i = 24; i < 32; i++) {
        value = (value << 8) | result->returnData.content[word * 32 + i];
    }
    return value;
}

// --- test_networkChainId ---
// MSTORE(0, CHAINID) RETURN(0, MSIZE)
static void child_chainId(void) {
    evmSetNetworkFetch();
    evmInit();
    block_t used;

    result_t result = callBlockContract();
    assert(returnWord(&result, 0) == 0x13a);
    assert(evmBlockUsed(&used) == BLOCK_BIT(chainId));
    assert(used.chainId == 0x13a);

    // cached, but still reported as used
    result = callBlockContract();
    assert(returnWord(&result, 0) == 0x13a);
    assert(evmBlockUsed(&used) == BLOCK_BIT(chainId));

    evmFinalize();
}

void test_networkChainId(void) {
    blockContractCode = "0x465f52595ff3";
    expectedStateTag = "\"0x100\"";
    with_mock_rpc(child_chainId, parent_block, 0, "");
    assert(chainIdRequests == 1);
    assert(blockNumberRequests == 1);
    assert(headerRequests == 0);
}

// --- test_networkNoBlockFetch ---
// PUSH0 PUSH0 RETURN reads nothing from the block
static void child_noBlockFetch(void) {
    evmSetNetworkFetch();
    evmInit();
    callBlockContract();
    block_t used;
    assert(evmBlockUsed(&used) == 0);
    evmFinalize();
}

void test_networkNoBlockFetch(void) {
    blockContractCode = "0x5f5ff3";
    expectedStateTag = "\"0x100\"";
    with_mock_rpc(child_noBlockFetch, parent_block, 0, "");
    assert(chainIdRequests == 0);
    assert(headerRequests == 0);
}

// --- test_networkHeader ---
// MSTORE(0, TIMESTAMP) GAS POP(BALANCE(COINBASE)) GAS SWAP1 SUB 32 MSTORE MSTORE(64, BASEFEE) RETURN(0, MSIZE)
static void child_header(void) {
    evmSetNetworkFetch();
    evmInit();

    result_t result = callBlockContract();
    assert(returnWord(&result, 0) == 0x6700);
    // the fetched coinbase is warm: COINBASE BALANCE POP GAS
    assert(returnWord(&result, 1) == G_BASE + G_ACCESS + G_BASE + G_BASE);
    assert(returnWord(&result, 2) == 1000000000);
    block_t used;
    assert(evmBlockUsed(&used) == (BLOCK_BIT(timestamp) | BLOCK_BIT(coinbase) | BLOCK_BIT(baseFee)));
    assert(used.timestamp == 0x6700);

    // cached; the coinbase is warm from the start
    result = callBlockContract();
    assert(returnWord(&result, 0) == 0x6700);
    assert(returnWord(&result, 1) == G_BASE + G_ACCESS + G_BASE + G_BASE);

    evmFinalize();
}

void test_networkHeader(void) {
    blockContractCode = "0x425f525a4131505a900360205248604052595ff3";
    expectedStateTag = "\"0x100\"";
    expectedHeaderNumber = "0x100";
    with_mock_rpc(child_header, parent_block, 0, "");
    assert(blockNumberRequests == 1);
    assert(headerRequests == 1);
    assert(minerRequests == 1);
}

// --- test_networkHeaderSkipsMiner ---
// MSTORE(0, TIMESTAMP) RETURN(0, MSIZE) fetches the header but not the miner's account
static void child_timestamp(void) {
    evmSetNetworkFetch();
    evmInit();
    result_t result = callBlockContract();
    assert(returnWord(&result, 0) == 0x6700);
    evmFinalize();
}

void test_networkHeaderSkipsMiner(void) {
    blockContractCode = "0x425f52595ff3";
    expectedStateTag = "\"0x100\"";
    expectedHeaderNumber = "0x100";
    with_mock_rpc(child_timestamp, parent_block, 0, "");
    assert(headerRequests == 1);
    assert(minerRequests == 0);
}

// --- test_networkNumberOverride ---
// Overriding number to N fetches state at N - 1 and the header of N, for that request only.
static void child_numberOverride(void) {
    evmSetNetworkFetch();
    evmInit();
    block_t overrides;
    overrides.number = 0x200;
    evmOverrideBlock(&overrides, BLOCK_BIT(number));

    result_t result = callBlockContract();
    assert(returnWord(&result, 0) == 0x6700);
    block_t used;
    assert(evmBlockUsed(&used) & BLOCK_BIT(timestamp));
    evmFinalize();
}

void test_networkNumberOverride(void) {
    blockContractCode = "0x425f525a4131505a900360205248604052595ff3";
    expectedStateTag = "\"0x1ff\"";
    expectedHeaderNumber = "0x200";
    with_mock_rpc(child_numberOverride, parent_block, 0, "");
    assert(blockNumberRequests == 0);
    assert(headerRequests == 1);
}

// --- test_networkCreateTargetNotFetched ---
// A CREATE target starts empty even when the chain has code there.
#define CREATED_ADDRESS "0xa0bcb2140dce5cf8dd708c6c2174248b8e4279c0"
static int createdRequests;

static void parent_create(FILE *req, FILE *rsp) {
    char buf[8192];
    createdRequests = 0;
    while (fgets(buf, sizeof(buf), req)) {
        if (strstr(buf, CREATED_ADDRESS)) {
            createdRequests++;
        }
        if (strstr(buf, "eth_blockNumber")) {
            fputs("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x100\"}\n", rsp);
        } else if (strstr(buf, "eth_getStorageAt")) {
            fputs("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x1\"}\n", rsp);
        } else if (strstr(buf, CREATED_ADDRESS)) {
            batch_response(rsp, "0x0102030405060708090a0b", "0x10");
        } else {
            batch_response(rsp, "0x", "0x0");
        }
        fflush(rsp);
    }
}

static void child_create(void) {
    evmSetNetworkFetch();
    evmInit();
    address_t from = AddressFromHex42("0x1111111111111111111111111111111111111111");
    evmMockNonce(from, 5);
    op_t initcode[] = {
        ADDRESS, EXTCODESIZE, PUSH0, MSTORE,
        PUSH0, SLOAD, PUSH1, 0x20, MSTORE,
        PUSH1, 0x40, PUSH0, RETURN,
    };
    data_t input;
    input.content = initcode;
    input.size = sizeof(initcode);
    val_t value;
    value[0] = value[1] = value[2] = 0;
    result_t result = txCreate(from, 100000, value, input);
    address_t created = AddressFromUint256(&result.status);
    address_t expected = AddressFromHex42(CREATED_ADDRESS);
    assert(AddressEqual(&expected, &created));
    assert(result.returnData.size == 64);
    for (int i = 0; i < 64; i++) {
        assert(result.returnData.content[i] == 0);
    }
    evmFinalize();
}

void test_networkCreateTargetNotFetched(void) {
    with_mock_rpc(child_create, parent_create, 0, "");
    assert(createdRequests == 0);
}

// --- test_networkMissingHeader ---
// A future block has no header, so its fields fall back to the defaults while NUMBER keeps the override.
static void child_missingHeader(void) {
    evmSetNetworkFetch();
    evmInit();
    block_t overrides;
    overrides.number = 0x200;
    evmOverrideBlock(&overrides, BLOCK_BIT(number));
    result_t result = callBlockContract();
    assert(returnWord(&result, 0) == 0x65712600);
    assert(returnWord(&result, 1) == 0x200);
    evmFinalize();
}

void test_networkMissingHeader(void) {
    blockContractCode = "0x425f5243602052595ff3";
    expectedStateTag = "\"0x1ff\"";
    expectedHeaderNumber = "0x200";
    headerMissing = true;
    with_mock_rpc(child_missingHeader, parent_block, 0, "evm: network: block 0x200 not found; using default header values\n");
    headerMissing = false;
    assert(headerRequests == 1);
}

int main(void) {
    test_networkFetchStorage();
    test_networkFetchStorageRpcError();
    test_networkFetchAccount();
    test_networkChainId();
    test_networkNoBlockFetch();
    test_networkHeader();
    test_networkHeaderSkipsMiner();
    test_networkNumberOverride();
    test_networkCreateTargetNotFetched();
    test_networkMissingHeader();
    return 0;
}
