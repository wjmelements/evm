#include "address.h"
#include "uint256.h"


address_t ec_recover(uint256_t msg_hash, unsigned char v, uint256_t r, uint256_t s);
