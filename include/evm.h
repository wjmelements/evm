#include <stddef.h>
#include <stdint.h>

typedef char address_t[20];
typedef char hash_t[32];
typedef char val_t[80];

typedef struct data {
    size_t size;
    char *content;
} data_t;

typedef struct callResult {
    hash_t status;
    data_t returnData;
} result_t;


void evmInit();
void evmMockCall(address_t to, val_t value, data_t inputData, result_t result);
void evmMockStorage(address_t to, hash_t key, hash_t storedValue);
void evmFinalize();

result_t evmCall(address_t from, uint64_t gas, address_t *to, val_t value, data_t input);
