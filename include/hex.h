#include <stdint.h>

#define HEXCHARS \
HEX(a) \
HEX(A) \
HEX(b) \
HEX(B) \
HEX(c) \
HEX(C) \
HEX(d) \
HEX(D) \
HEX(e) \
HEX(E) \
HEX(f) \
HEX(F) \
HEX(0) \
HEX(1) \
HEX(2) \
HEX(3) \
HEX(4) \
HEX(5) \
HEX(6) \
HEX(7) \
HEX(8) \
HEX(9) \

static inline uint8_t hexString8ToUint8(const uint8_t hexString8) {
    switch (hexString8) {
        #define HEX(c) case #c[0]: return 0x##c;
        HEXCHARS
        #undef HEX
    }
}
static inline uint8_t hexString16ToUint8(const char *hexString16) {
    return hexString8ToUint8(hexString16[0]) * 16 + hexString8ToUint8(hexString16[1]);
}
