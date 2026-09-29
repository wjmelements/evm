#include <stdint.h>
#include <stdio.h>

#include "address.h"

typedef uint64_t block_u64_t;
typedef uint256_t block_u256_t;
typedef address_t block_address_t;

// BLOCK_FIELD(name, type, override key, header key, config key, offline default)
// The override key is geth's blockOverrides key, except chainId, which is a top-level request key.
// Header fields are fetched together by eth_getBlockByNumber; number by eth_blockNumber; chainId by eth_chainId.
#define BLOCK_FIELDS \
        BLOCK_FIELD(number, u64, "number", NULL, "blockNumber", "0x13a2228") \
        BLOCK_FIELD(timestamp, u64, "time", "timestamp", "timestamp", "0x65712600") \
        BLOCK_FIELD(gasLimit, u64, "gasLimit", "gasLimit", "gasLimit", "0x1c9c380") \
        BLOCK_FIELD(chainId, u64, "chainId", NULL, "chainId", "0x1") \
        BLOCK_FIELD(baseFee, u256, "baseFeePerGas", "baseFeePerGas", "baseFee", "0x7") \
        BLOCK_FIELD(blobBaseFee, u256, "blobBaseFee", NULL, "blobBaseFee", "0x1") \
        BLOCK_FIELD(prevRandao, u256, "prevRandao", "mixHash", "prevRandao", "0x0") \
        BLOCK_FIELD(coinbase, address, "feeRecipient", "miner", "coinbase", "0x4838B106FCe9647Bdf1E7877BF73cE8B0BAD5f97")

typedef struct block {
#define BLOCK_FIELD(name, type, ...) block_ ## type ## _t name;
    BLOCK_FIELDS
#undef BLOCK_FIELD
} block_t;

enum {
#define BLOCK_FIELD(name, ...) BLOCK_ ## name ## _INDEX,
    BLOCK_FIELDS
#undef BLOCK_FIELD
    BLOCK_FIELD_COUNT
};

// one bit per field
typedef uint8_t blockFields_t;
#define BLOCK_BIT(name) ((blockFields_t)1 << BLOCK_ ## name ## _INDEX)
#define BLOCK_ALL ((blockFields_t)((1 << BLOCK_FIELD_COUNT) - 1))
#define BLOCK_HEADER (BLOCK_BIT(timestamp) | BLOCK_BIT(gasLimit) | BLOCK_BIT(baseFee) | BLOCK_BIT(prevRandao) | BLOCK_BIT(coinbase))

extern const char *const blockOverrideKey[BLOCK_FIELD_COUNT];
extern const char *const blockHeaderKey[BLOCK_FIELD_COUNT];
extern const char *const blockConfigKey[BLOCK_FIELD_COUNT];

// Parse a JSON-RPC quantity ("0x" optional, len hex chars) into the field at index
void blockParseField(block_t *block, uint8_t index, const char *hex, size_t len);
// Print the field at index as a quoted JSON-RPC quantity
void fprintBlockField(FILE *file, const block_t *block, uint8_t index);
// Returns the index of the field whose key in keys matches, or BLOCK_FIELD_COUNT
uint8_t blockKeyIndex(const char *const keys[BLOCK_FIELD_COUNT], const char *key, size_t len);
void blockDefaults(block_t *block);
