#include <stdint.h>
#include <stdio.h>

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
	if ('0' <= hexString8 && hexString8 <= '9') {
		return hexString8 - '0';
	}
	if ('a' <= hexString8 && hexString8 <= 'f') {
		return hexString8 - 'a';
	}
	if ('A' <= hexString8 && hexString8 <= 'F') {
		return hexString8 - 'A';
	}
    fputs("hexString8ToUint8: unexpected hex string", stderr);
    fputc(hexString8, stderr);
    fputc('\n', stderr);
    return 0xff;
}
static inline uint8_t hexString16ToUint8(const char *hexString16) {
    return hexString8ToUint8(hexString16[0]) * 16 + hexString8ToUint8(hexString16[1]);
}
static inline int isHex(char h) {
	if ('0' <= h && h <= '9') {
		return 1;
	}
	if ('a' <= h && h <= 'f') {
		return 1;
	}
	if ('A' <= h && h <= 'F') {
		return 1;
	}
	return 0;
}
