#define DECCHARS \
DEC(0) \
DEC(1) \
DEC(2) \
DEC(3) \
DEC(4) \
DEC(5) \
DEC(6) \
DEC(7) \
DEC(8) \
DEC(9) \

static inline int isDecimal(char h) {
    return h >= '0' && h <= '9';
}
