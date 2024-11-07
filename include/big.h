#include <stdint.h>

/* Size of 'digits' or 'words' used to represent big ints, you can set this to 1, 2, 4 or 8 */
#define bigWordSize 8

#ifndef bigWordSize
    #error Must define bigWordSize to be 1, 2, 4
#elif (bigWordSize == 1)
    /* Data type of array in structure */
    typedef uint8_t big_t;
    /* Data-type larger than big_t, for holding intermediate results of calculations */
    typedef uint32_t big_tmp_t;
#elif (bigWordSize == 2)
    typedef uint16_t big_t;
    typedef uint32_t big_tmp_t;
#elif (bigWordSize == 4)
    typedef uint32_t big_t;
    typedef uint64_t big_tmp_t;
#elif (bigWordSize == 8)
    typedef uint64_t big_t;
    typedef __uint128_t big_tmp_t;
#endif

/* Tokens returned by big_cmp() for value comparison */
enum { SMALLER = -1, EQUAL = 0, LARGER = 1 };

/* In/out functions */ 
void big_zero(size_t NumWords, big_t * big);
void big_from_int(size_t NumWords, big_t * big, big_tmp_t Integer);
int  big_to_int(size_t NumWords, const big_t * big);
void big_from_string(size_t NumWords, big_t * big, const char * Str); /* From decimal string */
void big_from_hex(size_t NumWords, big_t * big, const char * Str); /* From hex string */
void big_to_hex(size_t NumWords, const big_t * big, char * Str); /* To hex string */
void big_copy(size_t NumWords, big_t * dst, const big_t * src);
void big_copy_dif(size_t DstNumWords, big_t * Dst, size_t SrcNumWords, big_t * Src); /* Copy different sized ones */

/* basic arithmetic operations: */
void big_add(size_t aNumWords, const big_t * a, size_t bNumWords, const big_t * b, size_t outNumWords, big_t * out); /* out = a + b */
void big_sub(size_t aNumWords, const big_t * a, size_t bNumWords, const big_t * b, size_t outNumWords, big_t * out); /* out = a - b */
void big_mul(size_t XWords, const big_t * X, size_t YWords, const big_t * Y, size_t outWords, big_t * out); /* Karatsuba multiplication */
void big_mul_basic(size_t NumWords, const big_t * a, const big_t * b, big_t * out); /* out = a * b, old method, faster for small numbers */
void big_div(size_t NumWords, const big_t * a, const big_t * b, big_t * out); /* out = a / b */
void big_mod(size_t NumWords, const big_t * a, const big_t * b, big_t * out); /* out = a % b */
void big_divmod(size_t NumWords, const big_t * a, const big_t * b, big_t * C, big_t * D); /* C = a/B, D = a%B */

/* bitwise operations: */
void big_and(size_t NumWords, const big_t * a, const big_t * b, big_t * out); /* out = a & b */
void big_or(size_t NumWords, const big_t * a, const big_t * b, big_t * out);  /* out = a | b */
void big_xor(size_t NumWords, const big_t * a, const big_t * b, big_t * out); /* out = a ^ b */
void big_lshift(size_t NumWords, big_t * b, int nbits); /* b = a << nbits */
void big_rshift(size_t NumWords, big_t * b, int nbits); /* b = a >> nbits */

/* Special operators and comparison */
int  big_cmp(size_t NumWords, const big_t * a, const big_t * b); /* Compare: returns LARGER, EQUAL or SMALLER */
int  big_is_zero(size_t NumWords, const big_t * big); /* For comparison with zero */
void big_inc(size_t NumWords, big_t * big); /* Increment: add one to big */
void big_dec(size_t NumWords, big_t * big); /* Decrement: subtract one from big */
void big_pow(size_t NumWords, const big_t * a, const big_t * b, big_t * out); /* Calculate a^B -- e.g. 2^10 => 1024 */
void big_isqrt(size_t NumWords, const big_t * a, big_t * b); /* Integer square root -- e.g. isqrt(5) => 2*/
size_t big_truncate(size_t bigWords, const big_t * big); /* How many digits are actually needed */
