#include "block.h"

#include <inttypes.h>
#include <string.h>

const char *const blockOverrideKey[BLOCK_FIELD_COUNT] = {
#define BLOCK_FIELD(name, type, overrideKey, ...) overrideKey,
    BLOCK_FIELDS
#undef BLOCK_FIELD
};

const char *const blockHeaderKey[BLOCK_FIELD_COUNT] = {
#define BLOCK_FIELD(name, type, overrideKey, headerKey, ...) headerKey,
    BLOCK_FIELDS
#undef BLOCK_FIELD
};

const char *const blockConfigKey[BLOCK_FIELD_COUNT] = {
#define BLOCK_FIELD(name, type, overrideKey, headerKey, configKey, ...) configKey,
    BLOCK_FIELDS
#undef BLOCK_FIELD
};

static void parse_uint64_t(uint64_t *field, const char *hex, size_t len) {
    *field = 0;
    for (size_t i = 0; i < len; i++) {
        *field = (*field << 4) | hexString8ToUint8(hex[i]);
    }
}

static void parse_uint256_t(uint256_t *field, const char *hex, size_t len) {
    clear256(field);
    for (size_t i = 0; i < len; i++) {
        shiftl256(field, 4, field);
        LOWER(LOWER_P(field)) |= hexString8ToUint8(hex[i]);
    }
}

static void parse_address_t(address_t *field, const char *hex, size_t len) {
    uint256_t value;
    parse_uint256_t(&value, hex, len);
    *field = AddressFromUint256(&value);
}

void blockParseField(block_t *block, uint8_t index, const char *hex, size_t len) {
    if (len >= 2 && hex[0] == '0' && hex[1] == 'x') {
        hex += 2;
        len -= 2;
    }
    switch (index) {
#define BLOCK_FIELD(name, type, ...) \
        case BLOCK_ ## name ## _INDEX: \
            parse_ ## type(&block->name, hex, len); \
            break;
    BLOCK_FIELDS
#undef BLOCK_FIELD
    }
}

#define BLOCK_PRINT_uint64_t(file, field) fprintf(file, "0x%" PRIx64, field)
#define BLOCK_PRINT_uint256_t(file, field) fprintCompact256(file, &field)
#define BLOCK_PRINT_address_t(file, field) fprintAddress(file, field)

void fprintBlockField(FILE *file, const block_t *block, uint8_t index) {
    fputc('"', file);
    switch (index) {
#define BLOCK_FIELD(name, type, ...) \
        case BLOCK_ ## name ## _INDEX: \
            BLOCK_PRINT_ ## type(file, block->name); \
            break;
    BLOCK_FIELDS
#undef BLOCK_FIELD
    }
    fputc('"', file);
}

uint8_t blockKeyIndex(const char *const keys[BLOCK_FIELD_COUNT], const char *key, size_t len) {
    uint8_t index = 0;
    for (; index < BLOCK_FIELD_COUNT; index++) {
        if (keys[index] && strlen(keys[index]) == len && memcmp(keys[index], key, len) == 0) {
            break;
        }
    }
    return index;
}

void blockDefaults(block_t *block) {
#define BLOCK_FIELD(name, type, overrideKey, headerKey, configKey, value) \
        blockParseField(block, BLOCK_ ## name ## _INDEX, value, sizeof(value) - 1);
    BLOCK_FIELDS
#undef BLOCK_FIELD
}
