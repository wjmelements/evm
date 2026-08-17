#include "evm.h"
#include "network.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Fork, wire pipes onto child stdin/stdout, run child() in child and
// parent(req,rsp) in parent, then assert child exited 0.
static void with_mock_rpc(void (*child)(void), void (*parent)(FILE *req, FILE *rsp)) {
    int req_pipe[2], rsp_pipe[2];
    assert(pipe(req_pipe) == 0);
    assert(pipe(rsp_pipe) == 0);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        close(req_pipe[0]);
        close(rsp_pipe[1]);
        dup2(rsp_pipe[0], STDIN_FILENO);
        dup2(req_pipe[1], STDOUT_FILENO);
        close(rsp_pipe[0]);
        close(req_pipe[1]);
        child();
        exit(0);
    }
    close(req_pipe[1]);
    close(rsp_pipe[0]);
    FILE *req = fdopen(req_pipe[0], "r");
    FILE *rsp = fdopen(rsp_pipe[1], "w");
    parent(req, rsp);
    fclose(rsp);
    fclose(req);
    int status;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
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
    evmInit();
    evmSetBlockNumber(0x100);
    evmSetNetworkFetch();

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
    with_mock_rpc(child_storage, parent_storage);
}

// --- test_networkFetchAccount ---
// Contract: PUSH20 <target> BALANCE PUSH0 MSTORE MSIZE PUSH0 RETURN
// target = 0x2222000000000000000000000000000000000002 (fetched from network)
static void child_account(void) {
    evmInit();
    evmSetBlockNumber(0x100);
    evmSetNetworkFetch();

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
    with_mock_rpc(child_account, parent_account);
}

int main(void) {
    test_networkFetchStorage();
    test_networkFetchAccount();
    return 0;
}
