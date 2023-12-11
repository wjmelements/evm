#include "uint256.h"

#include <assert.h>
#include <stdio.h>


void test_bitwise() {
    uint256_t a, b, c, d;

    LOWER(LOWER(a)) = 2ull;
    UPPER(LOWER(a)) = 3ull << 8;
    LOWER(UPPER(a)) = 5ull << 16;
    UPPER(UPPER(a)) = 7ull << 24;

    LOWER(LOWER(b)) = 11ull << 32;
    UPPER(LOWER(b)) = (13ull << 40) + (7ull << 8);
    LOWER(UPPER(b)) = 17ull << 48;
    UPPER(UPPER(b)) = 19ull << 56;

    LOWER(LOWER(c)) = 23ull;
    UPPER(LOWER(c)) = 29ull;
    LOWER(UPPER(c)) = 31ull;
    UPPER(UPPER(c)) = 37ull;

    and256(&a, &b, &d);
    assert((LOWER(LOWER(a)) & LOWER(LOWER(b))) == LOWER(LOWER(d)));
    assert((UPPER(LOWER(a)) & UPPER(LOWER(b))) == UPPER(LOWER(d)));
    assert((LOWER(UPPER(a)) & LOWER(UPPER(b))) == LOWER(UPPER(d)));
    assert((UPPER(UPPER(a)) & UPPER(UPPER(b))) == UPPER(UPPER(d)));

    and256(&a, &c, &d);
    assert((LOWER(LOWER(a)) & LOWER(LOWER(c))) == LOWER(LOWER(d)));
    assert((UPPER(LOWER(a)) & UPPER(LOWER(c))) == UPPER(LOWER(d)));
    assert((LOWER(UPPER(a)) & LOWER(UPPER(c))) == LOWER(UPPER(d)));
    assert((UPPER(UPPER(a)) & UPPER(UPPER(c))) == UPPER(UPPER(d)));

    or256(&a, &b, &d);
    assert((LOWER(LOWER(a)) | LOWER(LOWER(b))) == LOWER(LOWER(d)));
    assert((UPPER(LOWER(a)) | UPPER(LOWER(b))) == UPPER(LOWER(d)));
    assert((LOWER(UPPER(a)) | LOWER(UPPER(b))) == LOWER(UPPER(d)));
    assert((UPPER(UPPER(a)) | UPPER(UPPER(b))) == UPPER(UPPER(d)));

    or256(&a, &c, &d);
    assert((LOWER(LOWER(a)) | LOWER(LOWER(c))) == LOWER(LOWER(d)));
    assert((UPPER(LOWER(a)) | UPPER(LOWER(c))) == UPPER(LOWER(d)));
    assert((LOWER(UPPER(a)) | LOWER(UPPER(c))) == LOWER(UPPER(d)));
    assert((UPPER(UPPER(a)) | UPPER(UPPER(c))) == UPPER(UPPER(d)));

    xor256(&a, &b, &d);
    assert((LOWER(LOWER(a)) ^ LOWER(LOWER(b))) == LOWER(LOWER(d)));
    assert((UPPER(LOWER(a)) ^ UPPER(LOWER(b))) == UPPER(LOWER(d)));
    assert((LOWER(UPPER(a)) ^ LOWER(UPPER(b))) == LOWER(UPPER(d)));
    assert((UPPER(UPPER(a)) ^ UPPER(UPPER(b))) == UPPER(UPPER(d)));

    xor256(&a, &c, &d);
    assert((LOWER(LOWER(a)) ^ LOWER(LOWER(c))) == LOWER(LOWER(d)));
    assert((UPPER(LOWER(a)) ^ UPPER(LOWER(c))) == UPPER(LOWER(d)));
    assert((LOWER(UPPER(a)) ^ LOWER(UPPER(c))) == LOWER(UPPER(d)));
    assert((UPPER(UPPER(a)) ^ UPPER(UPPER(c))) == UPPER(UPPER(d)));

    xor256(&a, &b, &a);
    xor256(&a, &b, &b);
    xor256(&a, &b, &a);
    assert(LOWER(LOWER(b)) == 2ull);
    assert(UPPER(LOWER(b)) == 3ull << 8);
    assert(LOWER(UPPER(b)) == 5ull << 16);
    assert(UPPER(UPPER(b)) == 7ull << 24);
    assert(LOWER(LOWER(a)) == 11ull << 32);
    assert(UPPER(LOWER(a)) == (13ull << 40) + (7ull << 8));
    assert(LOWER(UPPER(a)) == 17ull << 48);
    assert(UPPER(UPPER(a)) == 19ull << 56);

    clear256(&d);
    assert(LOWER(LOWER(d)) == 0);
    assert(UPPER(LOWER(d)) == 0);
    assert(LOWER(UPPER(d)) == 0);
    assert(UPPER(UPPER(d)) == 0);

    assert(!zero256(&a));
    assert(!zero256(&b));
    assert(!zero256(&c));
    assert(zero256(&d));
}

void test_bits() {
    uint256_t a;
    clear256(&a);
    assert(bits256(&a) == 0);

    LOWER(LOWER(a)) = 1;
    assert(bits256(&a) == 1);

    LOWER(UPPER(a)) = 1;
    assert(bits128(&UPPER(a)) == 1);
    assert(bits256(&a) == 129);

    UPPER(UPPER(a)) = 0xff00000000000000;
    assert(bits128(&UPPER(a)) == 128);
    assert(bits256(&a) == 256);

    UPPER(UPPER(a)) = 0x7f00000000000000;
    assert(bits128(&UPPER(a)) == 127);
    assert(bits256(&a) == 255);
}

void test_math() {
    uint256_t a, b, c, d, e;

    LOWER(LOWER(a)) = 2ull;
    UPPER(LOWER(a)) = 3ull;
    LOWER(UPPER(a)) = 5ull;
    UPPER(UPPER(a)) = 7ull;

    LOWER(LOWER(b)) = 11ull;
    UPPER(LOWER(b)) = 13ull;
    LOWER(UPPER(b)) = 17ull;
    UPPER(UPPER(b)) = 19ull;

    add256(&a, &b, &d);
    assert(LOWER(LOWER(d)) == 13ull);
    assert(UPPER(LOWER(d)) == 16ull);
    assert(LOWER(UPPER(d)) == 22ull);
    assert(UPPER(UPPER(d)) == 26ull);

    minus256(&d, &b, &c);
    assert(equal256(&a, &c));

    clear128(&UPPER(a));
    clear128(&UPPER(b));
    mul256(&a, &b, &d);
    assert(!zero256(&d));
    divmod256(&d, &b, &c, &e);
    assert(zero256(&e));
    assert(equal256(&c, &a));

    // 10e425c56daffabc35c1
    clear128(&UPPER(a));
    assert(UPPER(UPPER(a)) == 0);
    assert(LOWER(UPPER(a)) == 0);
    UPPER(LOWER(a)) = 0x10e4;
    LOWER(LOWER(a)) = 0x25c56daffabc35c1;
    mul256(&a, &a, &a);
    // 11d500bfaf40ac5044981798db5fb39f2c17b81
    assert(UPPER(UPPER(a)) == 0);
    assert(LOWER(UPPER(a)) == 0x11d500b);
    assert(UPPER(LOWER(a)) == 0xfaf40ac504498179);
    assert(LOWER(LOWER(a)) == 0x8db5fb39f2c17b81);
}

void test_shiftar() {
    uint256_t a;

    UPPER(UPPER(a)) = 0x8000000000000000;
    LOWER(UPPER(a)) = 0x8000000000000000;
    UPPER(LOWER(a)) = 0x8000000000000000;
    LOWER(LOWER(a)) = 0x8000000000000000;

    shiftar256(&a, 0, &a);

    assert(UPPER(UPPER(a)) == 0x8000000000000000);
    assert(LOWER(UPPER(a)) == 0x8000000000000000);
    assert(UPPER(LOWER(a)) == 0x8000000000000000);
    assert(LOWER(LOWER(a)) == 0x8000000000000000);

    shiftar256(&a, 1, &a);

    assert(UPPER(UPPER(a)) == 0xc000000000000000);
    assert(LOWER(UPPER(a)) == 0x4000000000000000);
    assert(UPPER(LOWER(a)) == 0x4000000000000000);
    assert(LOWER(LOWER(a)) == 0x4000000000000000);

    shiftar256(&a, 1, &a);

    assert(UPPER(UPPER(a)) == 0xe000000000000000);
    assert(LOWER(UPPER(a)) == 0x2000000000000000);
    assert(UPPER(LOWER(a)) == 0x2000000000000000);
    assert(LOWER(LOWER(a)) == 0x2000000000000000);

    shiftar256(&a, 1, &a);

    assert(UPPER(UPPER(a)) == 0xf000000000000000);
    assert(LOWER(UPPER(a)) == 0x1000000000000000);
    assert(UPPER(LOWER(a)) == 0x1000000000000000);
    assert(LOWER(LOWER(a)) == 0x1000000000000000);

    shiftar256(&a, 60, &a);

    assert(UPPER(UPPER(a)) == 0xffffffffffffffff);
    assert(LOWER(UPPER(a)) == 0x0000000000000001);
    assert(UPPER(LOWER(a)) == 0x0000000000000001);
    assert(LOWER(LOWER(a)) == 0x0000000000000001);

    shiftar256(&a, 60, &a);

    assert(UPPER(UPPER(a)) == 0xffffffffffffffff);
    assert(LOWER(UPPER(a)) == 0xfffffffffffffff0);
    assert(UPPER(LOWER(a)) == 0x0000000000000010);
    assert(LOWER(LOWER(a)) == 0x0000000000000010);

    shiftar256(&a, 128, &a);

    assert(UPPER(UPPER(a)) == 0xffffffffffffffff);
    assert(LOWER(UPPER(a)) == 0xffffffffffffffff);
    assert(UPPER(LOWER(a)) == 0xffffffffffffffff);
    assert(LOWER(LOWER(a)) == 0xfffffffffffffff0);

    shiftar256(&a, 1, &a);

    assert(UPPER(UPPER(a)) == 0xffffffffffffffff);
    assert(LOWER(UPPER(a)) == 0xffffffffffffffff);
    assert(UPPER(LOWER(a)) == 0xffffffffffffffff);
    assert(LOWER(LOWER(a)) == 0xfffffffffffffff8);

    UPPER(UPPER(a)) = 0x8760000000000000;
    LOWER(UPPER(a)) = 0x0800000000000000;
    UPPER(LOWER(a)) = 0x0080000000000000;
    LOWER(LOWER(a)) = 0x0008000000000000;

    shiftar256(&a, 196, &a);

    assert(UPPER(UPPER(a)) == 0xffffffffffffffff);
    assert(LOWER(UPPER(a)) == 0xffffffffffffffff);
    assert(UPPER(LOWER(a)) == 0xffffffffffffffff);
    assert(LOWER(LOWER(a)) == 0xf876000000000000);

    UPPER(UPPER(a)) = 0x8000000000000000;
    LOWER(UPPER(a)) = 0x0000000000000000;
    UPPER(LOWER(a)) = 0x0000000000000000;
    LOWER(LOWER(a)) = 0x0000000000000000;

    shiftar256(&a, 256, &a);

    assert(UPPER(UPPER(a)) == 0xffffffffffffffff);
    assert(LOWER(UPPER(a)) == 0xffffffffffffffff);
    assert(UPPER(LOWER(a)) == 0xffffffffffffffff);
    assert(LOWER(LOWER(a)) == 0xffffffffffffffff);
}

void test_exp() {
    uint256_t a, b, c;

    clear256(&a);
    LOWER(LOWER(a)) = 3;
    clear256(&b);
    LOWER(LOWER(b)) = 8;
    clear256(&c);

    exp256(&b, &c, &c);
    assert(LOWER(LOWER(c)) == 1);
    assert(UPPER(LOWER(c)) == 0);
    assert(LOWER(UPPER(c)) == 0);
    assert(UPPER(UPPER(c)) == 0);

    exp256(&a, &b, &c);
    assert(LOWER(LOWER(c)) == 6561);
    assert(UPPER(LOWER(c)) == 0);
    assert(LOWER(UPPER(c)) == 0);
    assert(UPPER(UPPER(c)) == 0);

    exp256(&b, &a, &c);
    assert(LOWER(LOWER(c)) == 512);
    assert(UPPER(LOWER(c)) == 0);
    assert(LOWER(UPPER(c)) == 0);
    assert(UPPER(UPPER(c)) == 0);

    LOWER(LOWER(c)) = 160;

    exp256(&a, &c, &c);
    assert(UPPER(UPPER(c)) == 0x304d37f120d696c8);
    assert(LOWER(UPPER(c)) == 0x34550e63d9bb9c14);
    assert(UPPER(LOWER(c)) == 0xb4f9165c9ede434e);
    assert(LOWER(LOWER(c)) == 0x4644e3998d6db881);
}

void test_signextend() {
    // first, test positive sign extension
    uint256_t a, b;
    UPPER(UPPER(a)) = 0x7777777777777777;
    LOWER(UPPER(a)) = 0x7777777777777777;
    UPPER(LOWER(a)) = 0x7777777777777777;
    LOWER(LOWER(a)) = 0x7777777777777777;

    signextend256(&a, 128, &b);
    assert(UPPER(UPPER(b)) == 0);
    assert(LOWER(UPPER(b)) == 0);
    assert(UPPER(LOWER(b)) == 0x7777777777777777);
    assert(LOWER(LOWER(b)) == 0x7777777777777777);

    signextend256(&a, 192, &b);
    assert(UPPER(UPPER(b)) == 0);
    assert(LOWER(UPPER(b)) == 0x7777777777777777);
    assert(UPPER(LOWER(b)) == 0x7777777777777777);
    assert(LOWER(LOWER(b)) == 0x7777777777777777);

    signextend256(&a, 200, &b);
    assert(UPPER(UPPER(b)) == 0x77);
    assert(LOWER(UPPER(b)) == 0x7777777777777777);
    assert(UPPER(LOWER(b)) == 0x7777777777777777);
    assert(LOWER(LOWER(b)) == 0x7777777777777777);

    // NOTE unsure if this is the correct specification
    signextend256(&a, 0, &b);
    assert(UPPER(UPPER(b)) == 0x0000000000000000);
    assert(LOWER(UPPER(b)) == 0x0000000000000000);
    assert(UPPER(LOWER(b)) == 0x0000000000000000);
    assert(LOWER(LOWER(b)) == 0x0000000000000000);

    // next, test negative sign extension
    signextend256(&a, 199, &b);
    assert(UPPER(UPPER(b)) == 0xfffffffffffffff7);
    assert(LOWER(UPPER(b)) == 0x7777777777777777);
    assert(UPPER(LOWER(b)) == 0x7777777777777777);
    assert(LOWER(LOWER(b)) == 0x7777777777777777);

    signextend256(&a, 1, &b);
    assert(UPPER(UPPER(b)) == 0xffffffffffffffff);
    assert(LOWER(UPPER(b)) == 0xffffffffffffffff);
    assert(UPPER(LOWER(b)) == 0xffffffffffffffff);
    assert(LOWER(LOWER(b)) == 0xffffffffffffffff);
}

void test_mulmod() {
    uint256_t a, b, c, d;

    UPPER(UPPER(a)) = 0xeeeeeeeeeeeeeeee;
    LOWER(UPPER(a)) = 0xeeeeeeeeeeeeeeee;
    UPPER(LOWER(a)) = 0xeeeeeeeeeeeeeeee;
    LOWER(LOWER(a)) = 0xeeeeeeeeeeeeeeee;
    UPPER(UPPER(b)) = 0xeeeeeeeeeeeeeeee;
    LOWER(UPPER(b)) = 0xeeeeeeeeeeeeeeee;
    UPPER(LOWER(b)) = 0xeeeeeeeeeeeeeeee;
    LOWER(LOWER(b)) = 0xeeeeeeeeeeeeeeee;
    UPPER(UPPER(c)) = 0xdddddddddddddddd;
    LOWER(UPPER(c)) = 0xdddddddddddddddd;
    UPPER(LOWER(c)) = 0xdddddddddddddddd;
    LOWER(LOWER(c)) = 0xdddddddddddddddd;

    mulmod256(&a, &b, &c, &d);
    assert(UPPER(UPPER(d)) == 0x1111111111111111);
    assert(UPPER(LOWER(d)) == 0x1111111111111111);
    assert(LOWER(UPPER(d)) == 0x1111111111111111);
    assert(LOWER(LOWER(d)) == 0x1111111111111111);

    mulmod256(&a, &b, &a, &d);
    assert(UPPER(UPPER(d)) == 0x0000000000000000);
    assert(UPPER(LOWER(d)) == 0x0000000000000000);
    assert(LOWER(UPPER(d)) == 0x0000000000000000);
    assert(LOWER(LOWER(d)) == 0x0000000000000000);
}

int main() {
    test_bitwise();
    test_bits();
    test_math();
    test_shiftar();
    test_exp();
    test_signextend();
    test_mulmod();
    return 0;
}
