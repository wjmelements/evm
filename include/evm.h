#include <stddef.h>
#include <stdint.h>

#include "address.h"
#include "uint256.h"

typedef uint32_t val_t[3];

typedef struct data {
    size_t size;
    uint8_t *content;
} data_t;

typedef struct callResult {
    data_t returnData;
    // status is the result pushed onto the stack
    // for CREATE it is the address or zero on failure
    // for CALL it is 1 on success and 0 on failure
    uint256_t status;
    uint64_t gasRemaining;
} result_t;


void evmInit();
void evmMockBalance(address_t to, val_t balance);
void evmMockCall(address_t to, val_t value, data_t inputData, result_t result);
void evmMockStorage(address_t to, uint256_t key, uint256_t storedValue);
void evmFinalize();

result_t evmCall(address_t from, uint64_t gas, address_t to, val_t value, data_t input);
result_t evmCreate(address_t from, uint64_t gas, val_t value, data_t input);
