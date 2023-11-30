#include "address.h"

#include <assert.h>

int main() {
    address_t address = AddressFromHex40("123456789a123456789a123456789a123456789a");
    address_t equal = AddressFromHex42("0x123456789a123456789a123456789a123456789a");
    assert(AddressEqual(&address, &equal));
    assert(address.address[0] == 0x12);
    assert(address.address[9] == 0x9a);
    assert(address.address[10] == 0x12);
    assert(address.address[19] == 0x9a);
    uint256_t stacked;
    AddressToUint256(&stacked, &address);
    address_t unstacked = AddressFromUint256(&stacked);
    assert(AddressEqual(&address, &unstacked));

    return 0;
}
