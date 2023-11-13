typedef char address_t[20];

#define AddressCopy(dst, src) memcpy(dst, src, 20)

static inline int AddressEqual(const address_t expected, const address_t actual) {
    for (int i = 0; i < 20; i ++) {
        if (expected[i] != actual[i]) {
            return 0;
        }
    }
    return 1;
}
