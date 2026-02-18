#include <secp256k1_recovery.h>
#include <stddef.h>
#include <stdint.h>

#include "address.h"
#include "data.h"
#include "keccak.h"
#include "ops.h"
#include "uint256.h"

typedef uint32_t val_t[3];

static inline void fprintVal(FILE* file, val_t val) {
    fputc('0', stderr);
    fputc('x', stderr);
    int nonzero = false;
    for (size_t i = 0; i < 3; i++) {
        if (nonzero) {
            fprintf(file, "%08x", val[i]);
        } else if (val[i]) {
            fprintf(file, "%x", val[i]);
            nonzero = true;
        }
    }
}

static inline int ValueIsZero(val_t val) {
    return val[0] == 0 && val[1] == 0 && val[2] == 0;
}

typedef struct codeChanges {
    data_t before;
    data_t after;
    struct codeChanges *prev;
} codeChanges_t;

typedef struct storageChanges {
    uint256_t key;
    uint256_t before;
    uint256_t after;
    uint64_t warm;
    struct storageChanges *prev;
} storageChanges_t;

typedef struct logChanges {
    uint256_t *topics; // in reverse order
    uint8_t topicCount;
    uint16_t logIndex;
    data_t data;
    struct logChanges *prev;
} logChanges_t;

static int LogsEqual(const logChanges_t *expectedLog, const logChanges_t *actualLog) {
    while (expectedLog && actualLog) {
        if (!DataEqual(&expectedLog->data, &actualLog->data)) {
            return false;
        }
        if (expectedLog->topicCount != actualLog->topicCount) {
            return false;
        }
        for (uint16_t i = 0; i < expectedLog->topicCount; i++) {
            if (!equal256(expectedLog->topics + i, actualLog->topics + i)) {
                return false;
            }
        }
        expectedLog = expectedLog->prev;
        actualLog = actualLog->prev;
    }
    return expectedLog == NULL && actualLog == NULL;
}

// state changes are reverted on failure and returned on success
typedef struct stateChanges {
    address_t account;
    // TODO balance change
    codeChanges_t *codeChanges; // LIFO
    // TODO selfdestruct
    // TODO nonce
    logChanges_t *logChanges; // LIFO
    // TODO warm
    storageChanges_t *storageChanges; // LIFO
    struct stateChanges *next;
} stateChanges_t;

// returns the number of items printed
uint16_t fprintLogs(FILE *, const stateChanges_t *, int showLogIndex);
uint16_t fprintLog(FILE *, const logChanges_t *, int showLogIndex);
uint16_t fprintLogDiff(FILE *, const logChanges_t *, const logChanges_t *, int showLogIndex);

typedef struct callResult {
    data_t returnData;
    // status is the result pushed onto the stack
    // for CREATE it is the address or zero on failure
    // for CALL it is 1 on success and 0 on failure
    uint256_t status;
    uint64_t gasRemaining;
    stateChanges_t *stateChanges;
} result_t;


void evmInit();
void evmFinalize();

#define EVM_DEBUG_STACK 1
#define EVM_DEBUG_MEMORY 2
#define EVM_DEBUG_OPS (4 + 8 + 16)
#define EVM_DEBUG_GAS 8
#define EVM_DEBUG_PC 16
#define EVM_DEBUG_CALLS 32
#define EVM_DEBUG_LOGS 64
void evmSetDebug(uint64_t flags);
void evmSetBlockNumber(uint64_t blockNumber);
void evmSetTimestamp(uint64_t timestamp);

void evmMockBalance(address_t to, const val_t balance);
void evmMockCall(address_t to, val_t value, data_t inputData, result_t result);
void evmMockStorage(address_t to, const uint256_t *key, const uint256_t *storedValue);
void evmMockNonce(address_t to, uint64_t nonce);
void evmMockCode(address_t to, data_t code);

typedef struct accessListStorage {
    uint256_t key;
    struct accessListStorage *prev;
} accessListStorage_t;

typedef struct accessList {
    address_t address;
    accessListStorage_t *storage;
    struct accessList *prev;
} accessList_t;

result_t evmConstruct(address_t from, address_t to, uint64_t gas, val_t value, data_t input);
// TODO gasPrice, basefee, blockNumber
result_t txCall(address_t from, uint64_t gas, address_t to, val_t value, data_t input, const accessList_t *accessList);
// TODO accessList
result_t txCreate(address_t from, uint64_t gas, val_t value, data_t input/*, const accessList_t *accessList*/);
