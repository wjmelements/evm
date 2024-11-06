#include "ec.h"
#include "keccak.h"

// ECC: yy = xxx+ax+b

// secp256k1
// A = 0, B = 7
// The order aka N = 0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141
static uint256_t SECP256K1_A = {
    {
        {
            {
                0,
                0
            }
        },
        {
            {
                0,
                0
            }
        }
    }
};
static uint256_t SECP256K1_B = {
    {
        {
            {
                0,
                0
            }
        },
        {
            {
                0,
                7
            }
        }
    }
};
static uint256_t SECP256K1_N = {
    {
        {
            {
                0xffffffffffffffff,
                0xfffffffffffffffe
            }
        },
        {
            {
                0xbaaedce6af48a03b,
                0xbfd25e8cd0364141
            }
        }
    }
};


address_t ec_recover(uint256_t msg_hash, unsigned char v, uint256_t r, uint256_t s) {
    // TODO
    address_t undefined;
    return undefined;
}
