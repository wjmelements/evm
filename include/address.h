#include "hex.h"
#include "precompiles.h"
#include "uint256.h"

#include <stdint.h>
#include <string.h>

// big-endian
typedef struct address {
    uint8_t address[20];
} __attribute__((__packed__)) address_t;

#define AddressCopy(dst, src) memcpy(dst.address, src.address, 20)

static inline int AddressEqual(const address_t *expected, const address_t *actual) {
    for (int i = 0; i < 20; i ++) {
        if (expected->address[i] != actual->address[i]) {
            return 0;
        }
    }
    return 1;
}

static inline address_t AddressFromHex40(const char *hex) {
    address_t address;
    for (uint8_t i = 0; i < 40; i += 2) {
        address.address[i >> 1] = hexString16ToUint8(hex+i);
    }
    return address;
}

#define AddressFromHex42(hex) AddressFromHex40((hex)+2)

static inline address_t AddressFromUint256(uint256_t *from) {
    address_t address;
    uint64_t scratch = LOWER(UPPER_P(from));
    address.address[0] = scratch >> 24;
    address.address[1] = scratch >> 16;
    address.address[2] = scratch >> 8;
    address.address[3] = scratch;

    scratch = UPPER(LOWER_P(from));
    address.address[4] = scratch >> 56;
    address.address[5] = scratch >> 48;
    address.address[6] = scratch >> 40;
    address.address[7] = scratch >> 32;
    address.address[8] = scratch >> 24;
    address.address[9] = scratch >> 16;
    address.address[10] = scratch >> 8;
    address.address[11] = scratch;

    scratch = LOWER(LOWER_P(from));
    address.address[12] = scratch >> 56;
    address.address[13] = scratch >> 48;
    address.address[14] = scratch >> 40;
    address.address[15] = scratch >> 32;
    address.address[16] = scratch >> 24;
    address.address[17] = scratch >> 16;
    address.address[18] = scratch >> 8;
    address.address[19] = scratch;

    return address;
}

static inline void AddressToUint256(uint256_t *dst, address_t *src) {
    UPPER(UPPER_P(dst)) = 0;
    LOWER(UPPER_P(dst)) =
        ( ((uint64_t)src->address[0] << 24ull)
        | ((uint64_t)src->address[1] << 16ull)
        | ((uint64_t)src->address[2] << 8ull)
        | ((uint64_t)src->address[3])
    );
    UPPER(LOWER_P(dst)) =
        ( ((uint64_t)src->address[4] << 56ull)
        | ((uint64_t)src->address[5] << 48ull)
        | ((uint64_t)src->address[6] << 40ull)
        | ((uint64_t)src->address[7] << 32ull)
        | ((uint64_t)src->address[8] << 24ull)
        | ((uint64_t)src->address[9] << 16ull)
        | ((uint64_t)src->address[10] << 8ull)
        | ((uint64_t)src->address[11])
    );
    LOWER(LOWER_P(dst)) =
        ( ((uint64_t)src->address[12] << 56ull)
        | ((uint64_t)src->address[13] << 48ull)
        | ((uint64_t)src->address[14] << 40ull)
        | ((uint64_t)src->address[15] << 32ull)
        | ((uint64_t)src->address[16] << 24ull)
        | ((uint64_t)src->address[17] << 16ull)
        | ((uint64_t)src->address[18] << 8ull)
        | ((uint64_t)src->address[19])
    );
}

#define fprintAddress(file, addr) fprintf(file, "0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", \
        addr.address[0], addr.address[1], addr.address[2], addr.address[3], addr.address[4], addr.address[5], addr.address[6], addr.address[7], addr.address[8], addr.address[9], addr.address[10],\
        addr.address[11], addr.address[12], addr.address[13], addr.address[14], addr.address[15], addr.address[16], addr.address[17], addr.address[18], addr.address[19]\
)


static inline int AddressIsPrecompile(const address_t address) {
    uint32_t a = *(uint32_t *)(address.address),
        b = *(uint32_t *)(address.address + 4),
        c = *(uint32_t *)(address.address + 8),
        d = *(uint32_t *)(address.address + 12),
        e = *(uint32_t *)(address.address + 16);
    return !a && !b && !c && !d && e < PRECOMPILE_BOUNDARY;
}

// assumes AddressIsPrecompile
static inline precompile_t AddressToPrecompile(const address_t address) {
    return *(precompile_t *)(address.address + (20 - sizeof(precompile_t)));
}

// assuming address is a precompile, determine whether it is known
static inline int PrecompileIsKnownPrecompile(const address_t address) {
    return address.address[19] < KNOWN_PRECOMPILES;
}
