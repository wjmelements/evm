#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "big.h"



void test_big_zero() {
    big_t param[3];
    param[0] = 1;
    param[1] = 1;
    param[2] = 1;
    big_zero(3, param);
    assert(!param[0]);
    assert(!param[1]);
    assert(!param[2]);
}


void test_big_hex() {
    const char testcase[33] = "0000000000abcdefABCDEF0123456789";
    big_t param[2];
    big_from_hex(2, param, testcase);
    assert(param[0] == 0xABCDEF0123456789);
    assert(param[1] == 0xabcdef);


    const char expected[33] = "0000000000abcdefabcdef0123456789";
    assert(strlen(expected) == 32);
    char actual[33];
    big_to_hex(2, param, actual);
    assert(strlen(actual) == 32);
    assert(strcmp(actual, expected) == 0);
}

void test_big_mul() {
    const big_t param[2] = {7, 11};
    big_t karatsuba[4] = {1,2,3,4};
    big_mul(2, param, 2, param, 4, karatsuba);
    const big_t expected[4] = {49, 154, 121, 0};
    assert(big_cmp(4, expected, karatsuba) == 0);
}

int main() {
    test_big_zero();
    test_big_hex();
    test_big_mul();
    return 0;
}
