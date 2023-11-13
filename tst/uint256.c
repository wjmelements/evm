#include "uint256.h"

#include <assert.h>


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
}


int main() {
    test_bitwise();
    test_math();
    return 0;
}
