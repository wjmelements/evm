#include "evm.h"
#include "json.h"

// The hex chars of the JSON string from val to end (just past its closing quote), without "0x"
const char *jsonHex(const char *val, const char *end, const char *key, size_t *len);
address_t jsonAddress(const char *val, const char *end, const char *key);

// Apply an eth_call state override set: {address: {balance, nonce, code, state | stateDiff}}
// Fields not overridden are fetched; a nonce override for from must match the call's nonce, if any
void applyStateOverrides(const char *json, address_t from, const uint64_t *nonce);
