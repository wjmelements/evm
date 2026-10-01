#include "overrides.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *jsonHex(const char *val, const char *end, const char *key, size_t *len) {
    if (*val != '"' || end[-1] != '"') {
        fprintf(stderr, "evm: \"%s\" must be a string\n", key);
        exit(1);
    }
    val++;
    end--;
    if (val[0] == '0' && val[1] == 'x') {
        val += 2;
    }
    *len = end - val;
    return val;
}

address_t jsonAddress(const char *val, const char *end, const char *key) {
    size_t len;
    const char *hex = jsonHex(val, end, key, &len);
    if (len != 40) {
        fprintf(stderr, "evm: malformed \"%s\" address\n", key);
        exit(1);
    }
    return AddressFromHex40(hex);
}

// The hex chars of a JSON key, without "0x"
static const char *keyHex(const char *key, size_t *len) {
    if (*len >= 2 && key[0] == '0' && key[1] == 'x') {
        key += 2;
        *len -= 2;
    }
    return key;
}

static void parseUint256(uint256_t *value, const char *hex, size_t len, const char *key) {
    while (len && *hex == '0') {
        hex++;
        len--;
    }
    if (len > 64) {
        fprintf(stderr, "evm: stateOverrides \"%s\" exceeds 256 bits\n", key);
        exit(1);
    }
    clear256(value);
    for (size_t i = 0; i < len; i++) {
        shiftl256(value, 4, value);
        LOWER(LOWER_P(value)) |= hexString8ToUint8(hex[i]);
    }
}

static void applyStorageOverrides(address_t address, const char *json) {
    if (*json != '{') {
        fputs("evm: stateOverrides storage must be an object\n", stderr);
        exit(1);
    }
    const char *key, *val;
    size_t klen;
    for (const char *end = json; (end = jNextKeyVal(end, &key, &klen, &val)); ) {
        uint256_t slot, value;
        const char *hex = keyHex(key, &klen);
        parseUint256(&slot, hex, klen, "storage key");
        size_t len;
        hex = jsonHex(val, end, "storage value", &len);
        parseUint256(&value, hex, len, "storage value");
        evmMockStorage(address, &slot, &value);
    }
}

static void applyAccountOverride(address_t address, const char *json, address_t from, const uint64_t *callNonce) {
    accountFields_t overridden = 0;
    data_t code;
    uint64_t nonce = 0;
    val_t balance = {0, 0, 0};
    const char *state = NULL;
    const char *stateDiff = NULL;
    const char *key, *val;
    size_t klen;
    for (const char *end = json; (end = jNextKeyVal(end, &key, &klen, &val)); ) {
        const char *hex;
        size_t len;
        switch (klen) {
        case 4:
            if (!memcmp(key, "code", 4)) {
                hex = jsonHex(val, end, "code", &len);
                if (len & 1) {
                    fputs("evm: odd-lengthed code override\n", stderr);
                    exit(1);
                }
                code.size = len / 2;
                code.content = code.size ? malloc(code.size) : NULL;
                for (size_t i = 0; i < code.size; i++) {
                    code.content[i] = hexString16ToUint8(hex + i * 2);
                }
                overridden |= ACCOUNT_CODE;
                continue;
            }
            break;
        case 5:
            if (!memcmp(key, "nonce", 5)) {
                hex = jsonHex(val, end, "nonce", &len);
                nonce = 0;
                for (size_t i = 0; i < len; i++) {
                    nonce = (nonce << 4) | hexString8ToUint8(hex[i]);
                }
                overridden |= ACCOUNT_NONCE;
                continue;
            } else if (!memcmp(key, "state", 5)) {
                state = val;
                continue;
            }
            break;
        case 7:
            if (!memcmp(key, "balance", 7)) {
                hex = jsonHex(val, end, "balance", &len);
                while (len && *hex == '0') {
                    hex++;
                    len--;
                }
                if (len > 24) {
                    fputs("evm: balance override exceeds 96 bits\n", stderr);
                    exit(1);
                }
                for (size_t i = 0; i < len; i++) {
                    balance[0] = (balance[0] << 4) | (balance[1] >> 28);
                    balance[1] = (balance[1] << 4) | (balance[2] >> 28);
                    balance[2] = (balance[2] << 4) | hexString8ToUint8(hex[i]);
                }
                overridden |= ACCOUNT_BALANCE;
                continue;
            }
            break;
        case 9:
            if (!memcmp(key, "stateDiff", 9)) {
                stateDiff = val;
                continue;
            }
            break;
        }
        fprintf(stderr, "evm: unsupported stateOverrides key \"%.*s\"\n", (int)klen, key);
        exit(1);
    }
    if (state && stateDiff) {
        fputs("evm: stateOverrides account has both \"state\" and \"stateDiff\"\n", stderr);
        exit(1);
    }
    if (callNonce && (overridden & ACCOUNT_NONCE) && *callNonce != nonce && AddressEqual(&address, &from)) {
        fputs("evm: \"nonce\" conflicts with the stateOverrides nonce of \"from\"\n", stderr);
        exit(1);
    }

    evmLoadAccount(address, overridden);
    if (overridden & ACCOUNT_CODE) {
        evmMockCode(address, code);
        free(code.content);
    }
    if (overridden & ACCOUNT_NONCE) {
        evmMockNonce(address, nonce);
    }
    if (overridden & ACCOUNT_BALANCE) {
        evmMockBalance(address, balance);
    }
    if (state) {
        evmClearStorage(address);
        applyStorageOverrides(address, state);
    } else if (stateDiff) {
        applyStorageOverrides(address, stateDiff);
    }
}

void applyStateOverrides(const char *json, address_t from, const uint64_t *nonce) {
    if (*json != '{') {
        fputs("evm: stateOverrides must be an object\n", stderr);
        exit(1);
    }
    const char *key, *val;
    size_t klen;
    for (const char *end = json; (end = jNextKeyVal(end, &key, &klen, &val)); ) {
        if (klen >= 2 && key[0] == '0' && key[1] == 'x') {
            key += 2;
            klen -= 2;
        }
        if (klen != 40) {
            fputs("evm: malformed stateOverrides address\n", stderr);
            exit(1);
        }
        if (*val != '{') {
            fputs("evm: stateOverrides account must be an object\n", stderr);
            exit(1);
        }
        applyAccountOverride(AddressFromHex40(key), val, from, nonce);
    }
}
