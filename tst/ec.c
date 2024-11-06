#include "ec.h"

#include <assert.h>


int main() {
    unsigned char v = '\0';
    uint256_t r, s, msg_hash;
    // b09c845f71e9fe89f49ce417a7bea2d475301f4e9a7946bfdeb56cd3e0789a14
    UPPER(UPPER(r)) = 0xb09c845f71e9fe89;
    LOWER(UPPER(r)) = 0xf49ce417a7bea2d4;
    UPPER(LOWER(r)) = 0x75301f4e9a7946bf;
    LOWER(LOWER(r)) = 0xdeb56cd3e0789a14;
    // 1375cfdf52ce77a99518c4ecc6b149125acec750559146255815a74f1fb8a1f0
    UPPER(UPPER(s)) = 0x1375cfdf52ce77a9;
    LOWER(UPPER(s)) = 0x9518c4ecc6b14912;
    UPPER(LOWER(s)) = 0x5acec75055914625;
    LOWER(LOWER(s)) = 0x5815a74f1fb8a1f0;
    // 384538b7967eb8b39f8c3e16ecef9805e25e0c87d3e0a73cdb6968c229e3af5a
    UPPER(UPPER(msg_hash)) = 0x384538b7967eb8b3;
    LOWER(UPPER(msg_hash)) = 0x9f8c3e16ecef9805;
    UPPER(LOWER(msg_hash)) = 0xe25e0c87d3e0a73c;
    LOWER(LOWER(msg_hash)) = 0xdb6968c229e3af5a;


    address_t expected = AddressFromHex42("0x4a6f6B9fF1fc974096f9063a45Fd12bD5B928AD1");
    address_t actual = ec_recover(msg_hash, v, r, s);
    assert(
        AddressEqual(
            &expected,
            &actual
        )
    );
    return 0;
}
