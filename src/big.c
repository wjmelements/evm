#include <string.h>
#include <alloca.h>

// This is adapted from https://github.com/ilia3101/Big-Integer-C

#include "big.h"

/* Printing format strings */
#ifndef bigWordSize
    #error Must define bigWordSize to be 1, 2, 4
#elif (bigWordSize == 1)
    /* Max value of integer type */
    #define MAX_VAL ((big_tmp_t)0xFF)
#elif (bigWordSize == 2)
    #define MAX_VAL ((big_tmp_t)0xFFFF)
#elif (bigWordSize == 4)
    #define MAX_VAL ((big_tmp_t)0xFFFFFFFF)
#elif (bigWordSize == 8)
    #define MAX_VAL ((big_tmp_t)0xFFFFFFFFFFFFFFFF)
#endif

/* Bad macros */
#define MIN(A,B) (((A)<(B))?(A):(B))
#define MAX(A,B) (((A)>(B))?(A):(B))

/* Functions for shifting number in-place. */
static void _lshift_one_bit(size_t NumWords, big_t * A);
static void _rshift_one_bit(size_t NumWords, big_t * A);
static void _lshift_word(size_t NumWords, big_t * A, int nwords);
static void _rshift_word(size_t NumWords, big_t * A, int nwords);

/* Endianness issue if machine is not little-endian? */
#ifdef bigWordSize
#if (bigWordSize == 1)
#define big_FROM_INT(big, Integer) { \
    ((big_t *)(void *)big)[0] = (((big_tmp_t)Integer) & 0x000000ff); \
    ((big_t *)(void *)big)[1] = (((big_tmp_t)Integer) & 0x0000ff00) >> 8; \
    ((big_t *)(void *)big)[2] = (((big_tmp_t)Integer) & 0x00ff0000) >> 16; \
    ((big_t *)(void *)big)[3] = (((big_tmp_t)Integer) & 0xff000000) >> 24; \
}
#elif (bigWordSize == 2)
#define big_FROM_INT(big, Integer) { \
    ((big_t *)(void *)big)[0] = (((big_tmp_t)Integer) & 0x0000ffff); \
    ((big_t *)(void *)big)[1] = (((big_tmp_t)Integer) & 0xffff0000) >> 16; \
}
#elif (bigWordSize == 4)
#define big_FROM_INT(big, Integer) { \
    ((big_t *)(void *)big)[0] = ((big_tmp_t)Integer); \
    ((big_t *)(void *)big)[1] = ((big_tmp_t)Integer) >> ((big_tmp_t)32); \
}
#elif (bigWordSize == 8)
#define big_FROM_INT(big, Integer) { \
    ((big_t *)(void *)big)[0] = ((big_tmp_t)Integer); \
    ((big_t *)(void *)big)[1] = ((big_tmp_t)Integer) >> ((big_tmp_t)64); \
}
#endif
#endif

/* Public / Exported functions. */
void big_zero(size_t NumWords, big_t * big)
{
    for (size_t i = 0; i < NumWords; ++i) {
        big[i] = 0;
    }
}

void big_from_int(size_t NumWords, big_t * big, big_tmp_t Integer)
{
    big_zero(NumWords, big);
    big_FROM_INT(big, Integer);
}

int big_to_int(size_t NumWords, const big_t * big)
{
    int ret = 0;

/* Endianness issue if machine is not little-endian? */
#if (bigWordSize == 1)
    ret += big[0];
    ret += big[1] << 8;
    ret += big[2] << 16;
    ret += big[3] << 24;
#elif (bigWordSize == 2)
    ret += big[0];
    ret += big[1] << 16;
#elif (bigWordSize == 4)
    ret += big[0];
#elif (bigWordSize == 8)
    ret += big[0];
#endif

    return ret;
}

size_t big_truncate(size_t NumWords, const big_t * big)
{
    --NumWords;
    while (big[NumWords] == 0 && NumWords > 0) --NumWords;
    return ++NumWords;
}

void big_from_string(size_t NumWords, big_t * big, const char * str)
{
    big_zero(NumWords, big);

    big_t * temp = alloca(NumWords*bigWordSize);
    big_t digit;
    big_t ten = 10;

    while (*str != 0)
    {
        big_mul(NumWords, big, 1, &ten, NumWords, temp);

        digit = (*(str++)-'0');

        if (digit != 0)
            big_add(NumWords, temp, 1, &digit, NumWords, big);
        else
            big_copy(NumWords, big, temp);
    }
}

static big_t hex_to_word(const char * Text, int Length)
{
    big_t word = 0;
    for (int i = 0; i < Length; ++i)
    {
        char character = Text[i];
        word <<= 4;
        if (character >= '0' && character <= '9')
            word += character - '0';
        else if (character <= 'F' && character >= 'A')
            word += character - 'A' + 10;
        else if (character <= 'f' && character >= 'a')
            word += character - 'a' + 10;
    }
    return word;
}

void big_from_hex(size_t NumWords, big_t * big, const char * Str)
{
    big_zero(NumWords, big);
    size_t length = strlen(Str);

    /* whole Words in this string */
    size_t num_words = length / (bigWordSize*2);
    if (num_words * (bigWordSize*2) < length) ++num_words; /* round up */

    const char * string_word = Str + length;

    for (size_t i = 0; i < num_words; ++i)
    {
        /* How many characters should be read from the string */
        size_t hex_length = MIN(bigWordSize*2, string_word-Str);
        string_word -= (bigWordSize*2);
        big[i] = hex_to_word(string_word, hex_length);
    }
}

void big_to_hex(size_t NumWords, const big_t * big, char * Str)
{
    NumWords = big_truncate(NumWords, big);

    size_t str_index = 0;

    for (int_fast32_t d = NumWords-1; d >= 0; --d)
    {
        big_t word = big[d];
        for (int big = 0; big < bigWordSize*2; ++big) {
            uint8_t nibble = (word >> (big_t)(big*4)) & 0x0F;
            char hexchar = (nibble <= 9) ? '0' + nibble : 'a' + nibble-10;
            Str[str_index+bigWordSize*2-1-big] = hexchar;
        }
        str_index += bigWordSize*2;
    }

    Str[str_index] = 0;
}

void big_dec(size_t NumWords, big_t * big)
{
    big_t tmp; /* copy of big */
    big_t res;

    for (size_t i = 0; i < NumWords; ++i) {
        tmp = big[i];
        res = tmp - 1;
        big[i] = res;

        if (!(res > tmp)) {
            break;
        }
    }
}

void big_inc(size_t NumWords, big_t * big)
{
    big_t res;
    big_tmp_t tmp; /* copy of big */

    for (size_t i = 0; i < NumWords; ++i) {
        tmp = big[i];
        res = tmp + 1;
        big[i] = res;

        if (res > tmp) {
            break;
        }
    }
}

void big_add(size_t AWords, const big_t * A, size_t BWords, const big_t * B, size_t Out_NumWords, big_t * Out)
{
    /* Make it so that A will be smaller than B */
    if (AWords > BWords)
    {
        size_t temp1 = BWords;
        BWords = AWords;
        AWords = temp1;
        const big_t * temp2 = B;
        B = A;
        A = temp2;
    }

    int loop_to = 0;
    size_t loop1 = 0;
    size_t loop2 = 0;
    size_t loop3 = 0;

    if (Out_NumWords <= AWords) {
        loop_to = 1;
        loop1 = Out_NumWords;
    }
    else if (Out_NumWords <= BWords) {
        loop_to = 2;
        loop1 = AWords;
        loop2 = Out_NumWords;
    }
    else {
        loop_to = 3;
        loop1 = AWords;
        loop2 = BWords;
        loop3 = Out_NumWords;
    }

    int carry = 0;
    big_tmp_t tmp;
    size_t i;

    for (i = 0; i < loop1; ++i)
    {
        tmp = (big_tmp_t)A[i] + B[i] + carry;
        carry = (tmp > MAX_VAL);
        Out[i] = (tmp & MAX_VAL);
    }

    if (loop_to == 1) return;

    for (; i < loop2; ++i)
    {
        tmp = (big_tmp_t)B[i] + 0 + carry;
        carry = (tmp > MAX_VAL);
        Out[i] = (tmp & MAX_VAL);
    }

    if (loop_to == 2) return;

    /* Do the carry, then fill the rest with zeros */
    Out[i++] = carry;
    for (; i < loop3; ++i) Out[i] = 0;
}

void big_sub(size_t AWords, const big_t * A, size_t BWords, const big_t * B, size_t Out_NumWords, big_t * Out)
{
    int loop_to = 0;
    size_t loop1 = 0;
    size_t loop2 = 0;
    size_t loop3 = 0;

    if (Out_NumWords <= MIN(AWords, BWords))
    {
        loop_to = 1;
        loop1 = MIN(AWords, BWords);
    }
    else if (Out_NumWords <= MAX(AWords, BWords))
    {
        loop_to = 2;
        loop1 = MIN(AWords, BWords);
        loop2 = Out_NumWords;
    }
    else {
        loop_to = 3;
        loop1 = AWords;
        loop2 = BWords;
        loop3 = Out_NumWords;
    }

    big_tmp_t res;
    big_tmp_t tmp1;
    big_tmp_t tmp2;
    int borrow = 0;
    size_t i;

    for (i = 0; i < loop1; ++i) {
        tmp1 = (big_tmp_t)A[i] + (MAX_VAL + 1); /* + number_base */
        tmp2 = (big_tmp_t)B[i] + borrow;
        ;
        res = (tmp1 - tmp2);
        Out[i] = (big_t)(res & MAX_VAL); /* "modulo number_base" == "%
            (number_base - 1)" if number_base is 2^N */
        borrow = (res <= MAX_VAL);
    }

    if (loop_to == 1) return;

    if (AWords > BWords)
    {
        for (; i < loop2; ++i) {
            tmp1 = (big_tmp_t)A[i] + (MAX_VAL + 1);
            tmp2 =  borrow;
            res = (tmp1 - tmp2);
            Out[i] = (big_t)(res & MAX_VAL);
            borrow = (res <= MAX_VAL);
        }
    }
    else
    {
        for (; i < loop2; ++i) {
            tmp1 = (big_tmp_t)MAX_VAL + 1;
            tmp2 = (big_tmp_t)B[i] + borrow;
            res = (tmp1 - tmp2);
            Out[i] = (big_t)(res & MAX_VAL);
            borrow = (res <= MAX_VAL);
        }
    }

    if (loop_to == 2) return;

    for (; i < loop3; ++i) {
        tmp1 = (big_tmp_t)0 + (MAX_VAL + 1);
        tmp2 = (big_tmp_t)0 + borrow;
        res = (tmp1 - tmp2);
        Out[i] = (big_t)(res & MAX_VAL);
        borrow = (res <= MAX_VAL);
    }
}

void big_mul_basic(size_t NumWords, const big_t * A, const big_t * B, big_t * Out)
{
    big_t * row = alloca(NumWords*bigWordSize);
    big_t * tmp = alloca(NumWords*bigWordSize);
    size_t i, j;

    big_zero(NumWords, Out);

    for (i = 0; i < NumWords; ++i) {
        big_zero(NumWords, row);

        for (j = 0; j < NumWords; ++j) {
            if (i + j < NumWords) {
                big_zero(NumWords, tmp);
                big_tmp_t intermediate = ((big_tmp_t)A[i] * (big_tmp_t)B[j]);
                big_from_int(NumWords, tmp, intermediate);
                _lshift_word(NumWords, tmp, i + j);
                big_add(NumWords, tmp, NumWords, row, NumWords, row);
            }
        }
        big_add(NumWords, Out, NumWords, row, NumWords, Out);
    }
}

/* Cool USSR algorithm for fast multiplication (THERE IS NOT A SINGLE 100% CORRECT PSEUDO CODE ONLINE) */
static void big_Karatsuba_internal(size_t num1_NumWords, const big_t * num1, size_t num2_NumWords, const big_t * num2, size_t Out_NumWords, big_t * Out, int rlevel) /* Out should be XWords + YWords in size to always avoid overflow */
{
    /* Optimise the size, to avoid any waste any resources */
    num1_NumWords = big_truncate(num1_NumWords, num1);
    num2_NumWords = big_truncate(num2_NumWords, num2);

    if (num1_NumWords == 0 || num2_NumWords == 0)
    {
        big_zero(Out_NumWords, Out);
        return;
    }
    if (num1_NumWords == 1 && num2_NumWords == 1)
    {
        big_tmp_t result = ((big_tmp_t)(*num1)) * ((big_tmp_t)(*num2));
        if (Out_NumWords == 2) { big_FROM_INT(Out, result); }
        else big_from_int(Out_NumWords, Out, result);
        return;
    }

    size_t m = MIN(num2_NumWords, num1_NumWords);
    size_t m2 = m / 2;
    /* do A round up, this is what stops infinite recursion when the inputs are size 1 and 2 */
    if ((m % 2) == 1) ++m2;

    /* low 1 */
    size_t low1_NumWords = m2;
    const big_t * low1 = num1;
    /* high 1 */
    size_t high1_NumWords = num1_NumWords - m2;
    const big_t * high1 = num1 + m2;
    /* low 2 */ 
    size_t low2_NumWords = m2;
    const big_t * low2 = num2;
    /* high 2 */
    size_t high2_NumWords = num2_NumWords - m2;
    const big_t * high2 = num2 + m2;

    // z0 = karatsuba(low1, low2)
    // z1 = karatsuba((low1 + high1), (low2 + high2))
    // z2 = karatsuba(high1, high2)
    size_t z0_NumWords = low1_NumWords + low2_NumWords;
    big_t * z0 = alloca(z0_NumWords*bigWordSize);
    size_t z1_NumWords = (MAX(low1_NumWords, high1_NumWords)+1) + (MAX(low2_NumWords, high2_NumWords)+1);
    big_t * z1 = alloca(z1_NumWords*bigWordSize);
    size_t z2_NumWords =  high1_NumWords + high2_NumWords;
    int use_out_as_z2 = (Out_NumWords >= z2_NumWords); /* Sometimes we can use Out to store z2, then we don't have to copy from z2 to out later (2X SPEEDUP!) */
    if (use_out_as_z2) {big_zero(Out_NumWords-(z2_NumWords),Out+z2_NumWords);}/* The remaining part of Out must be ZERO'D */
    big_t * z2 = (use_out_as_z2) ? Out : alloca(z2_NumWords*bigWordSize);

    /* Make z0 and z2 */
    big_Karatsuba_internal(low1_NumWords, low1, low2_NumWords, low2, z0_NumWords, z0, rlevel+1);
    big_Karatsuba_internal(high1_NumWords, high1, high2_NumWords, high2, z2_NumWords, z2, rlevel+1);

    /* make z1 */
    {
        size_t low1high1_NumWords = MAX(low1_NumWords, high1_NumWords)+1;
        size_t low2high2_NumWords = MAX(low2_NumWords, high2_NumWords)+1;
        big_t * low1high1 = alloca(low1high1_NumWords*bigWordSize);
        big_t * low2high2 = alloca(low2high2_NumWords*bigWordSize);
        big_add(low1_NumWords, low1, high1_NumWords, high1, low1high1_NumWords, low1high1);
        big_add(low2_NumWords, low2, high2_NumWords, high2, low2high2_NumWords, low2high2);
        big_Karatsuba_internal(low1high1_NumWords, low1high1, low2high2_NumWords, low2high2, z1_NumWords, z1, rlevel+1);
    }

    // return (z2 * 10 ^ (m2 * 2)) + ((z1 - z2 - z0) * 10 ^ m2) + z0
    big_sub(z1_NumWords, z1, z2_NumWords, z2, z1_NumWords, z1);
    big_sub(z1_NumWords, z1, z0_NumWords, z0, z1_NumWords, z1);
    if (!use_out_as_z2) big_copy_dif(Out_NumWords, Out, z2_NumWords, z2);
    _lshift_word(Out_NumWords, Out, m2);
    big_add(z1_NumWords, z1, Out_NumWords, Out, Out_NumWords, Out);
    _lshift_word(Out_NumWords, Out, m2);
    big_add(Out_NumWords, Out, z0_NumWords, z0, Out_NumWords, Out);
}

void big_mul(size_t ANumWords, const big_t * A, size_t BNumWords, const big_t * B, size_t OutNumWords, big_t * Out)
{
    big_Karatsuba_internal(ANumWords, A, BNumWords, B, OutNumWords, Out, 0);
}
 
void big_div(size_t NumWords, const big_t * A, const big_t * B, big_t * Out)
{
    big_t * current = alloca(NumWords*bigWordSize);
    big_t * denom = alloca(NumWords*bigWordSize);
    big_t * tmp = alloca(NumWords*bigWordSize);

    big_from_int(NumWords, current, 1); // int current = 1;
    big_copy(NumWords, denom, B); // denom = B
    big_copy(NumWords, tmp, A); // tmp   = A

    const big_tmp_t half_max = 1 + (big_tmp_t)(MAX_VAL / 2);
    int overflow = 0;
    while (big_cmp(NumWords, denom, A) != LARGER) // while (denom <= A) {
    {
        if (denom[NumWords - 1] >= half_max) {
            overflow = 1;
            break;
        }
        _lshift_one_bit(NumWords, current); //   current <<= 1;
        _lshift_one_bit(NumWords, denom); //   denom <<= 1;
    }
    if (!overflow) {
        _rshift_one_bit(NumWords, denom); // denom >>= 1;
        _rshift_one_bit(NumWords, current); // current >>= 1;
    }
    big_zero(NumWords, Out); // int answer = 0;

    while (!big_is_zero(NumWords, current)) // while (current != 0)
    {
        if (big_cmp(NumWords, tmp, denom) != SMALLER) //   if (dividend >= denom)
        {
            big_sub(NumWords, tmp, NumWords, denom, NumWords, tmp); //     dividend -= denom;
            big_or(NumWords, Out, current, Out); //     answer |= current;
        }
        _rshift_one_bit(NumWords, current); //   current >>= 1;
        _rshift_one_bit(NumWords, denom); //   denom >>= 1;
    }
}

void big_lshift(size_t NumWords, big_t * B, int nbits)
{
    /* Handle shift in multiples of word-size */
    const int nbits_pr_word = (bigWordSize * 8);
    int nwords = nbits / nbits_pr_word;
    if (nwords != 0) {
        _lshift_word(NumWords, B, nwords);
        nbits -= (nwords * nbits_pr_word);
    }

    if (nbits != 0) {
        size_t i;
        for (i = (NumWords - 1); i > 0; --i) {
            B[i] = (B[i] << nbits) | (B[i - 1] >> ((8 * bigWordSize) - nbits));
        }
        B[i] <<= nbits;
    }
}

void big_rshift(size_t NumWords, big_t * B, int nbits)
{
    /* Handle shift in multiples of word-size */
    const int nbits_pr_word = (bigWordSize * 8);
    int nwords = nbits / nbits_pr_word;
    if (nwords != 0) {
        _rshift_word(NumWords, B, nwords);
        nbits -= (nwords * nbits_pr_word);
    }

    if (nbits != 0) {
        size_t i;
        for (i = 0; i < (NumWords - 1); ++i) {
            B[i] = (B[i] >> nbits) | (B[i + 1] << ((8 * bigWordSize) - nbits));
        }
        B[i] >>= nbits;
    }
}

void big_mod(size_t NumWords, const big_t * A, const big_t * B, big_t * Out)
{
    /* Take divmod and throw away div part */
    big_t * tmp = alloca(NumWords*bigWordSize);
    big_divmod(NumWords, A, B, tmp, Out);
}

void big_divmod(size_t NumWords, const big_t * A, const big_t * B, big_t * C, big_t * D)
{
    big_t * tmp = alloca(NumWords*bigWordSize);

    /* Out = (A / B) */
    big_div(NumWords, A, B, C);

    /* tmp = (Out * B) */
    big_mul(NumWords, C, NumWords, B, NumWords, tmp);

    /* Out = A - tmp */
    big_sub(NumWords, A, NumWords, tmp, NumWords, D);
}

void big_and(size_t NumWords, const big_t * A, const big_t * B, big_t * Out)
{
    for (size_t i = 0; i < NumWords; ++i) {
        Out[i] = (A[i] & B[i]);
    }
}

void big_or(size_t NumWords, const big_t * A, const big_t * B, big_t * Out)
{
    for (size_t i = 0; i < NumWords; ++i) {
        Out[i] = (A[i] | B[i]);
    }
}

void big_xor(size_t NumWords, const big_t * A, const big_t * B, big_t * Out)
{
    for (size_t i = 0; i < NumWords; ++i) {
        Out[i] = (A[i] ^ B[i]);
    }
}

int big_cmp(size_t NumWords, const big_t * A, const big_t * B)
{
    size_t i = NumWords;
    do {
        i -= 1; /* Decrement first, to start with last array element */
        if (A[i] > B[i]) {
            return LARGER;
        } else if (A[i] < B[i]) {
            return SMALLER;
        }
    } while (i != 0);

    return EQUAL;
}

int big_is_zero(size_t NumWords, const big_t * big)
{
    for (size_t i = 0; i < NumWords; ++i) {
        if (big[i]) {
            return 0;
        }
    }

    return 1;
}

void big_pow(size_t NumWords, const big_t * A, const big_t * B, big_t * Out)
{
    big_zero(NumWords, Out);

    if (big_cmp(NumWords, B, Out) == EQUAL) {
        /* Return 1 when exponent is 0 -- big^0 = 1 */
        big_inc(NumWords, Out);
    } else {
        big_t * bcopy = alloca(NumWords*bigWordSize), * tmp = alloca(NumWords*bigWordSize);
        big_copy(NumWords, bcopy, B);

        /* Copy A -> tmp */
        big_copy(NumWords, tmp, A);

        big_dec(NumWords, bcopy);

        /* Begin summing products: */
        while (!big_is_zero(NumWords, bcopy)) {
            /* Out = tmp * tmp */
            big_mul(NumWords, tmp, NumWords, A, NumWords, Out);
            /* Decrement B by one */
            big_dec(NumWords, bcopy);

            big_copy(NumWords, tmp, Out);
        }

        /* Out = tmp */
        big_copy(NumWords, Out, tmp);
    }
}

void big_isqrt(size_t NumWords, const big_t * A, big_t * B)
{
    big_t * low = alloca(NumWords*bigWordSize);
    big_t * high = alloca(NumWords*bigWordSize);
    big_t * mid = alloca(NumWords*bigWordSize);
    big_t * tmp = alloca(NumWords*bigWordSize);

    big_zero(NumWords, low);
    big_copy(NumWords, high, A);
    big_copy(NumWords, mid, high);
    big_rshift(NumWords, mid, 1);
    big_inc(NumWords, mid);

    while (big_cmp(NumWords, high, low) > 0) {
        big_mul(NumWords, mid, NumWords, mid, NumWords, tmp);
        if (big_cmp(NumWords, tmp, A) > 0) {
            big_copy(NumWords, high, mid);
            big_dec(NumWords, high);
        } else {
            big_copy(NumWords, low, mid);
        }
        big_sub(NumWords, high, NumWords, low, NumWords, mid);
        _rshift_one_bit(NumWords, mid);
        big_add(NumWords, low, NumWords, mid, NumWords, mid);
        big_inc(NumWords, mid);
    }
    big_copy(NumWords, B, low);
}

void big_copy(size_t NumWords, big_t * Dst, const big_t * Src)
{
    for (size_t i = 0; i < NumWords; ++i) {
        Dst[i] = Src[i];
    }
}

void big_copy_dif(size_t DstNumWords, big_t * Dst, size_t SrcNumWords, big_t * Src)
{
    size_t smallest = (DstNumWords < SrcNumWords) ? DstNumWords : SrcNumWords;
    size_t i;
    for (i = 0; i < smallest; ++i) Dst[i] = Src[i];
    for (; i < DstNumWords; ++i) Dst[i] = 0;
}

/* Private / Static functions. */
static void _rshift_word(size_t NumWords, big_t * A, int nwords)
{
    size_t i;
    if (nwords >= NumWords) {
        for (i = 0; i < NumWords; ++i) {
            A[i] = 0;
        }
        return;
    }

    for (i = 0; i < NumWords - nwords; ++i) {
        A[i] = A[i + nwords];
    }
    for (; i < NumWords; ++i) {
        A[i] = 0;
    }
}

static void _lshift_word(size_t NumWords, big_t * A, int nwords)
{
    int_fast32_t i;
    /* Shift whole words */
    for (i = (NumWords - 1); i >= nwords; --i) {
        A[i] = A[i - nwords];
    }
    /* Zero pad shifted words. */
    for (; i >= 0; --i) {
        A[i] = 0;
    }
}

static void _lshift_one_bit(size_t NumWords, big_t * A)
{
    for (size_t i = (NumWords - 1); i > 0; --i) {
        A[i] = (A[i] << 1) | (A[i - 1] >> ((8 * bigWordSize) - 1));
    }
    A[0] <<= 1;
}

static void _rshift_one_bit(size_t NumWords, big_t * A)
{
    for (size_t i = 0; i < (NumWords - 1); ++i) {
        A[i] = (A[i] >> 1) | (A[i + 1] << ((8 * bigWordSize) - 1));
    }
    A[NumWords - 1] >>= 1;
}
