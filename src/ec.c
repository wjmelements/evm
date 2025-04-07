#include "ec.h"
#include "keccak.h"

// ECC: yy = xxx+ax+b

// secp256k1
// A = 0, B = 7
// The base point aka G = (0x79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798, 0x483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8)
// The order aka N = 0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141
// The prime aka P = 0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f
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
static uint256_t SECP256K1_GX = {
    {
        {
            {
                0x79be667ef9dcbbac,
                0x55a06295ce870b07
            }
        },
        {
            {
                0x029bfcdb2dce28d9,
                0x59f2815b16f81798
            }
        }
    }
};
static uint256_t SECP256K1_GY = {
    {
        {
            {
                0x483ada7726a3c465,
                0x5da4fbfc0e1108a8
            }
        },
        {
            {
                0xfd17b448a6855419,
                0x9c47d08ffb10d4b8
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
static uint256_t SECP256K1_P = {
    {
        {
            {
                0xffffffffffffffff,
                0xffffffffffffffff
            }
        },
        {
            {
                0xffffffffffffffff,
                0xfffffffefffffc2f
            }
        }
    }
};


address_t ec_recover(uint256_t msg_hash, unsigned char v, uint256_t r, uint256_t s) {
    uint256_t beta;
    mulmod256(&r, &r, &SECP256K1_P, &beta);
    mulmod256(&beta, &r, &SECP256K1_P, &beta);
    // NOTE A*X is skipped here because A=0
    add256(&beta, &SECP256K1_B, &beta);
    address_t undefined;
    return undefined;
}
