/*******************************************************************************
*   Ledger Ethereum App
*   (c) 2016-2019 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

// Adapted from https://github.com/calccrypto/uint256_t

#include <inttypes.h>
#include <stdlib.h>

#include "uint256.h"

static const char HEXDIGITS[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static uint64_t readUint64BE(const uint8_t *buffer) {
    return (((uint64_t)buffer[0]) << 56) | (((uint64_t)buffer[1]) << 48) |
           (((uint64_t)buffer[2]) << 40) | (((uint64_t)buffer[3]) << 32) |
           (((uint64_t)buffer[4]) << 24) | (((uint64_t)buffer[5]) << 16) |
           (((uint64_t)buffer[6]) << 8) | (((uint64_t)buffer[7]));
}

void readu128BE(const uint8_t *buffer, uint128_t *target) {
    UPPER_P(target) = readUint64BE(buffer);
    LOWER_P(target) = readUint64BE(buffer + 8);
}

void readu256BE(const uint8_t *buffer, uint256_t *target) {
    readu128BE(buffer, &UPPER_P(target));
    readu128BE(buffer + 16, &LOWER_P(target));
}

static void dumpUint64BE(uint64_t source, uint8_t *target) {
    target[0] = source >> 56;
    target[1] = source >> 48;
    target[2] = source >> 40;
    target[3] = source >> 32;
    target[4] = source >> 24;
    target[5] = source >> 16;
    target[6] = source >> 8;
    target[7] = source;
}

void dumpu128BE(const uint128_t *source, uint8_t *target) {
    dumpUint64BE(UPPER_P(source), target);
    dumpUint64BE(LOWER_P(source), target + 8);
}

void dumpu256BE(const uint256_t *source, uint8_t *target) {
    dumpu128BE(&UPPER_P(source), target);
    dumpu128BE(&LOWER_P(source), target + 16);
}

void fprint128(FILE *fp, const uint128_t *number) {
    fprintf(fp, "%016" PRIx64, UPPER_P(number));
    fprintf(fp, "%016" PRIx64, LOWER_P(number));
}

void fprint256(FILE *fp, const uint256_t *number) {
    fprint128(fp, &UPPER_P(number));
    fprint128(fp, &LOWER_P(number));
}

void fprint512(FILE *fp, const uint512_t *number) {
    fprint256(fp, &UPPER_P(number));
    fprint256(fp, &LOWER_P(number));
}

bool zero128(const uint128_t *number) {
    return ((LOWER_P(number) == 0) && (UPPER_P(number) == 0));
}

bool zero256(const uint256_t *number) {
    return (zero128(&LOWER_P(number)) && zero128(&UPPER_P(number)));
}

void copy128(uint128_t *target, const uint128_t *number) {
    UPPER_P(target) = UPPER_P(number);
    LOWER_P(target) = LOWER_P(number);
}

void copy256(uint256_t *target, const uint256_t *number) {
    copy128(&UPPER_P(target), &UPPER_P(number));
    copy128(&LOWER_P(target), &LOWER_P(number));
}

void copy512(uint512_t *target, const uint512_t *number) {
    copy256(&UPPER_P(target), &UPPER_P(number));
    copy256(&LOWER_P(target), &LOWER_P(number));
}

void clear128(uint128_t *target) {
    UPPER_P(target) = 0;
    LOWER_P(target) = 0;
}

void clear256(uint256_t *target) {
    clear128(&UPPER_P(target));
    clear128(&LOWER_P(target));
}

void clear512(uint512_t *target) {
    clear256(&UPPER_P(target));
    clear256(&LOWER_P(target));
}

void shiftl128(const uint128_t *number, uint32_t value, uint128_t *target) {
    if (value >= 128) {
        clear128(target);
    } else if (value == 64) {
        UPPER_P(target) = LOWER_P(number);
        LOWER_P(target) = 0;
    } else if (value == 0) {
        copy128(target, number);
    } else if (value < 64) {
        UPPER_P(target) =
            (UPPER_P(number) << value) + (LOWER_P(number) >> (64 - value));
        LOWER_P(target) = (LOWER_P(number) << value);
    } else if ((128 > value) && (value > 64)) {
        UPPER_P(target) = LOWER_P(number) << (value - 64);
        LOWER_P(target) = 0;
    } else {
        clear128(target);
    }
}

void shiftl256(const uint256_t *number, uint32_t value, uint256_t *target) {
    if (value >= 256) {
        clear256(target);
    } else if (value == 128) {
        copy128(&UPPER_P(target), &LOWER_P(number));
        clear128(&LOWER_P(target));
    } else if (value == 0) {
        copy256(target, number);
    } else if (value < 128) {
        uint128_t tmp1;
        uint128_t tmp2;
        uint256_t result;
        shiftl128(&UPPER_P(number), value, &tmp1);
        shiftr128(&LOWER_P(number), (128 - value), &tmp2);
        add128(&tmp1, &tmp2, &UPPER(result));
        shiftl128(&LOWER_P(number), value, &LOWER(result));
        copy256(target, &result);
    } else if ((256 > value) && (value > 128)) {
        shiftl128(&LOWER_P(number), (value - 128), &UPPER_P(target));
        clear128(&LOWER_P(target));
    } else {
        clear256(target);
    }
}

void shiftl512(const uint512_t *number, uint32_t value, uint512_t *target) {
    if (value >= 512) {
        clear512(target);
    } else if (value == 256) {
        copy256(&UPPER_P(target), &LOWER_P(number));
        clear256(&LOWER_P(target));
    } else if (value == 0) {
        copy512(target, number);
    } else if (value < 256) {
        uint256_t tmp1;
        uint256_t tmp2;
        uint512_t result;
        shiftl256(&UPPER_P(number), value, &tmp1);
        shiftr256(&LOWER_P(number), (256 - value), &tmp2);
        add256(&tmp1, &tmp2, &UPPER(result));
        shiftl256(&LOWER_P(number), value, &LOWER(result));
        copy512(target, &result);
    } else if ((512 > value) && (value > 256)) {
        shiftl256(&LOWER_P(number), (value - 256), &UPPER_P(target));
        clear256(&LOWER_P(target));
    } else {
        clear512(target);
    }
}

void shiftr128(const uint128_t *number, uint32_t value, uint128_t *target) {
    if (value >= 128) {
        clear128(target);
    } else if (value == 64) {
        UPPER_P(target) = 0;
        LOWER_P(target) = UPPER_P(number);
    } else if (value == 0) {
        copy128(target, number);
    } else if (value < 64) {
        uint128_t result;
        UPPER(result) = UPPER_P(number) >> value;
        LOWER(result) =
            (UPPER_P(number) << (64 - value)) + (LOWER_P(number) >> value);
        copy128(target, &result);
    } else if ((128 > value) && (value > 64)) {
        LOWER_P(target) = UPPER_P(number) >> (value - 64);
        UPPER_P(target) = 0;
    } else {
        clear128(target);
    }
}

void shiftr256(const uint256_t *number, uint32_t value, uint256_t *target) {
    if (value >= 256) {
        clear256(target);
    } else if (value == 128) {
        copy128(&LOWER_P(target), &UPPER_P(number));
        clear128(&UPPER_P(target));
    } else if (value == 0) {
        copy256(target, number);
    } else if (value < 128) {
        uint128_t tmp1;
        uint128_t tmp2;
        uint256_t result;
        shiftr128(&UPPER_P(number), value, &UPPER(result));
        shiftr128(&LOWER_P(number), value, &tmp1);
        shiftl128(&UPPER_P(number), (128 - value), &tmp2);
        add128(&tmp1, &tmp2, &LOWER(result));
        copy256(target, &result);
    } else if ((256 > value) && (value > 128)) {
        shiftr128(&UPPER_P(number), (value - 128), &LOWER_P(target));
        clear128(&UPPER_P(target));
    } else {
        clear256(target);
    }
}

void shiftr512(const uint512_t *number, uint32_t value, uint512_t *target) {
    if (value >= 512) {
        clear512(target);
    } else if (value == 256) {
        copy256(&LOWER_P(target), &UPPER_P(number));
        clear256(&UPPER_P(target));
    } else if (value == 0) {
        copy512(target, number);
    } else if (value < 256) {
        uint256_t tmp1;
        uint256_t tmp2;
        uint512_t result;
        shiftr256(&UPPER_P(number), value, &UPPER(result));
        shiftr256(&LOWER_P(number), value, &tmp1);
        shiftl256(&UPPER_P(number), (256 - value), &tmp2);
        add256(&tmp1, &tmp2, &LOWER(result));
        copy512(target, &result);
    } else if ((512 > value) && (value > 256)) {
        shiftr256(&UPPER_P(number), (value - 256), &LOWER_P(target));
        clear256(&UPPER_P(target));
    } else {
        clear512(target);
    }
}
void shiftar256(const uint256_t *number, uint32_t value, uint256_t *target) {
    bool positive = (UPPER(UPPER_P(number)) < 0x8000000000000000);
    shiftr256(number, value, target);
    if (positive) return;
    if (!value) return;
    if (value >= 64) {
        UPPER(UPPER_P(target)) = 0xffffffffffffffffllu;
        if (value >= 128) {
            LOWER(UPPER_P(target)) = 0xffffffffffffffffllu;
            if (value >= 192) {
                UPPER(LOWER_P(target)) = 0xffffffffffffffffllu;
                if (value >= 256) {
                    LOWER(LOWER_P(target)) = 0xffffffffffffffffllu;
                } else if (value > 192) {
                    LOWER(LOWER_P(target)) |= 0xffffffffffffffffllu - ((1llu << (256 - value)) - 1llu);
                }
            } else if (value > 128) {
                UPPER(LOWER_P(target)) |= 0xffffffffffffffffllu - ((1llu << (192 - value)) - 1llu);
            }
        } else if (value > 64) {
            LOWER(UPPER_P(target)) |= 0xffffffffffffffffllu - ((1llu << (128 - value)) - 1llu);
        }
    } else {
        UPPER(UPPER_P(target)) |= 0xffffffffffffffffllu - ((1llu << (64 - value)) - 1llu);
    }
}

uint32_t bits128(const uint128_t *number) {
    uint32_t result = 0;
    if (UPPER_P(number)) {
        result = 64;
        uint64_t up = UPPER_P(number);
        while (up) {
            up >>= 1;
            result++;
        }
    } else {
        uint64_t low = LOWER_P(number);
        while (low) {
            low >>= 1;
            result++;
        }
    }
    return result;
}

uint32_t bits256(const uint256_t *number) {
    if (!zero128(&UPPER_P(number))) {
        return 128 + bits128(&UPPER_P(number));
    } else {
        return bits128(&LOWER_P(number));
    }
}

uint32_t bits512(const uint512_t *number) {
    if (!zero256(&UPPER_P(number))) {
        return 256 + bits256(&UPPER_P(number));
    }
    return bits256(&LOWER_P(number));
}

static uint64_t clz64(uint64_t number) {
    // binary search
    uint64_t zeros = 0;
    if (!(number & 0xFFFFFFFF00000000)) { zeros += 32; number <<= 32; }
    if (!(number & 0xFFFF000000000000)) { zeros += 16; number <<= 16; }
    if (!(number & 0xFF00000000000000)) { zeros +=  8; number <<=  8; }
    if (!(number & 0xF000000000000000)) { zeros +=  4; number <<=  4; }
    if (!(number & 0xC000000000000000)) { zeros +=  2; number <<=  2; }
    if (!(number & 0x8000000000000000)) { zeros +=  1; }
    return zeros;
}

uint64_t clz256(const uint256_t *number) {
    if (UPPER(UPPER_P(number))) {
        return clz64(UPPER(UPPER_P(number)));
    }
    if (LOWER(UPPER_P(number))) {
        return 64 + clz64(LOWER(UPPER_P(number)));
    }
    if (UPPER(LOWER_P(number))) {
        return 128 + clz64(UPPER(LOWER_P(number)));
    }
    if (LOWER(LOWER_P(number))) {
        return 192 + clz64(LOWER(LOWER_P(number)));
    }
    return 256;
}

bool equal128(const uint128_t *number1, const uint128_t *number2) {
    return (UPPER_P(number1) == UPPER_P(number2)) &&
           (LOWER_P(number1) == LOWER_P(number2));
}

bool equal256(const uint256_t *number1, const uint256_t *number2) {
    return (equal128(&UPPER_P(number1), &UPPER_P(number2)) &&
            equal128(&LOWER_P(number1), &LOWER_P(number2)));
}

bool equal512(const uint512_t *number1, const uint512_t *number2) {
    return (equal256(&UPPER_P(number1), &UPPER_P(number2)) &&
            equal256(&LOWER_P(number1), &LOWER_P(number2)));
}

bool gt128(const uint128_t *number1, const uint128_t *number2) {
    if (UPPER_P(number1) == UPPER_P(number2)) {
        return (LOWER_P(number1) > LOWER_P(number2));
    }
    return (UPPER_P(number1) > UPPER_P(number2));
}

bool gt256(const uint256_t *number1, const uint256_t *number2) {
    if (equal128(&UPPER_P(number1), &UPPER_P(number2))) {
        return gt128(&LOWER_P(number1), &LOWER_P(number2));
    }
    return gt128(&UPPER_P(number1), &UPPER_P(number2));
}

bool gt512(const uint512_t *number1, const uint512_t *number2) {
    if (equal256(&UPPER_P(number1), &UPPER_P(number2))) {
        return gt256(&LOWER_P(number1), &LOWER_P(number2));
    }
    return gt256(&UPPER_P(number1), &UPPER_P(number2));
}
bool gte128(const uint128_t *number1, const uint128_t *number2) {
    return gt128(number1, number2) || equal128(number1, number2);
}

bool gte256(const uint256_t *number1, const uint256_t *number2) {
    return gt256(number1, number2) || equal256(number1, number2);
}

bool gte512(const uint512_t *number1, const uint512_t *number2) {
    return gt512(number1, number2) || equal512(number1, number2);
}


bool sgt256(const uint256_t *number1, const uint256_t *number2) {
    return (
        UPPER(UPPER_P(number1)) >= 0x8000000000000000 == UPPER(UPPER_P(number2)) >= 0x8000000000000000
        ? gt256(number1, number2)
        : gt256(number2, number1)
    );
}

void add128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    UPPER_P(target) =
        UPPER_P(number1) + UPPER_P(number2) +
        ((LOWER_P(number1) + LOWER_P(number2)) < LOWER_P(number1));
    LOWER_P(target) = LOWER_P(number1) + LOWER_P(number2);
}

void add256(const uint256_t *number1, const uint256_t *number2, uint256_t *target) {
    uint128_t tmp;
    add128(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    add128(&LOWER_P(number1), &LOWER_P(number2), &tmp);
    if (gt128(&LOWER_P(number1), &tmp)) {
        uint128_t one;
        UPPER(one) = 0;
        LOWER(one) = 1;
        add128(&UPPER_P(target), &one, &UPPER_P(target));
    }
    add128(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}

void add512(const uint512_t *number1, const uint512_t *number2, uint512_t *target) {
    uint256_t tmp;
    add256(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    add256(&LOWER_P(number1), &LOWER_P(number2), &tmp);
    if (gt256(&LOWER_P(number1), &tmp)) {
        uint256_t one;
        clear128(&UPPER(one));
        UPPER(LOWER(one)) = 0;
        LOWER(LOWER(one)) = 1;
        add256(&UPPER_P(target), &one, &UPPER_P(target));
    }
    copy256(&LOWER_P(target), &tmp);
}

void minus128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    UPPER_P(target) =
        UPPER_P(number1) - UPPER_P(number2) -
        ((LOWER_P(number1) - LOWER_P(number2)) > LOWER_P(number1));
    LOWER_P(target) = LOWER_P(number1) - LOWER_P(number2);
}

void minus256(const uint256_t *number1, const uint256_t *number2, uint256_t *target) {
    uint128_t tmp;
    minus128(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    minus128(&LOWER_P(number1), &LOWER_P(number2), &tmp);
    if (gt128(&tmp, &LOWER_P(number1))) {
        uint128_t one;
        UPPER(one) = 0;
        LOWER(one) = 1;
        minus128(&UPPER_P(target), &one, &UPPER_P(target));
    }
    minus128(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}

void minus512(const uint512_t *number1, const uint512_t *number2, uint512_t *target) {
    uint256_t tmp;
    minus256(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    minus256(&LOWER_P(number1), &LOWER_P(number2), &tmp);
    if (gt256(&tmp, &LOWER_P(number1))) {
        uint256_t one;
        clear128(&UPPER(one));
        UPPER(LOWER(one)) = 0;
        LOWER(LOWER(one)) = 1;
        minus256(&UPPER_P(target), &one, &UPPER_P(target));
    }
    minus256(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}
void or128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    UPPER_P(target) = UPPER_P(number1) | UPPER_P(number2);
    LOWER_P(target) = LOWER_P(number1) | LOWER_P(number2);
}

void or256(const uint256_t *number1, const uint256_t *number2, uint256_t *target) {
    or128(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    or128(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}

void or512(const uint512_t *number1, const uint512_t *number2, uint512_t *target) {
    or256(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    or256(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}

void and128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    UPPER_P(target) = UPPER_P(number1) & UPPER_P(number2);
    LOWER_P(target) = LOWER_P(number1) & LOWER_P(number2);
}

void and256(const uint256_t *number1, const uint256_t *number2, uint256_t *target) {
    and128(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    and128(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}

void xor128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    UPPER_P(target) = UPPER_P(number1) ^ UPPER_P(number2);
    LOWER_P(target) = LOWER_P(number1) ^ LOWER_P(number2);
}

void xor256(const uint256_t *number1, const uint256_t *number2, uint256_t *target) {
    xor128(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    xor128(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}

void not128(const uint128_t *number, uint128_t *target) {
    UPPER_P(target) = ~UPPER_P(number);
    LOWER_P(target) = ~LOWER_P(number);
}

void not256(const uint256_t *number, uint256_t *target) {
    not128(&UPPER_P(number), &UPPER_P(target));
    not128(&LOWER_P(number), &LOWER_P(target));
}

void mul128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    uint64_t top[4] = {UPPER_P(number1) >> 32, UPPER_P(number1) & 0xffffffff,
                       LOWER_P(number1) >> 32, LOWER_P(number1) & 0xffffffff};
    uint64_t bottom[4] = {UPPER_P(number2) >> 32, UPPER_P(number2) & 0xffffffff,
                          LOWER_P(number2) >> 32,
                          LOWER_P(number2) & 0xffffffff};
    uint64_t products[4][4];
    uint128_t tmp, tmp2;

    for (uint8_t y = 4; y --> 0;) {
        for (uint8_t x = 4; x --> 0;) {
            products[3 - x][y] = top[x] * bottom[y];
        }
    }

    uint64_t fourth32 = products[0][3] & 0xffffffff;
    uint64_t third32 = (products[0][2] & 0xffffffff) + (products[0][3] >> 32);
    uint64_t second32 = (products[0][1] & 0xffffffff) + (products[0][2] >> 32);
    uint64_t first32 = (products[0][0] & 0xffffffff) + (products[0][1] >> 32);

    third32 += products[1][3] & 0xffffffff;
    second32 += (products[1][2] & 0xffffffff) + (products[1][3] >> 32);
    first32 += (products[1][1] & 0xffffffff) + (products[1][2] >> 32);

    second32 += products[2][3] & 0xffffffff;
    first32 += (products[2][2] & 0xffffffff) + (products[2][3] >> 32);

    first32 += products[3][3] & 0xffffffff;

    UPPER(tmp) = first32 << 32;
    LOWER(tmp) = 0;
    UPPER(tmp2) = third32 >> 32;
    LOWER(tmp2) = third32 << 32;
    add128(&tmp, &tmp2, target);
    UPPER(tmp) = second32;
    LOWER(tmp) = 0;
    add128(&tmp, target, &tmp2);
    UPPER(tmp) = 0;
    LOWER(tmp) = fourth32;
    add128(&tmp, &tmp2, target);
}

void mul256(const uint256_t *number1, const uint256_t *number2, uint256_t *target) {
    uint128_t top[4];
    uint128_t bottom[4];
    UPPER(top[0]) = 0;
    LOWER(top[0]) = UPPER(UPPER_P(number1));
    UPPER(top[1]) = 0;
    LOWER(top[1]) = LOWER(UPPER_P(number1));
    UPPER(top[2]) = 0;
    LOWER(top[2]) = UPPER(LOWER_P(number1));
    UPPER(top[3]) = 0;
    LOWER(top[3]) = LOWER(LOWER_P(number1));
    UPPER(bottom[0]) = 0;
    LOWER(bottom[0]) = UPPER(UPPER_P(number2));
    UPPER(bottom[1]) = 0;
    LOWER(bottom[1]) = LOWER(UPPER_P(number2));
    UPPER(bottom[2]) = 0;
    LOWER(bottom[2]) = UPPER(LOWER_P(number2));
    UPPER(bottom[3]) = 0;
    LOWER(bottom[3]) = LOWER(LOWER_P(number2));

    uint128_t products[4][4];
    for (uint8_t y = 4; y --> 0;) {
        for (uint8_t x = 4; x --> 0;) {
            mul128(&top[x], &bottom[y], &products[x][y]);
        }
    }

    uint128_t tmp, tmp2, fourth64, third64, second64, first64;
    UPPER(fourth64) = 0;
    LOWER(fourth64) = LOWER(products[3][3]);
    UPPER(tmp) = 0;
    LOWER(tmp) = LOWER(products[3][2]);
    UPPER(tmp2) = 0;
    LOWER(tmp2) = UPPER(products[3][3]);
    add128(&tmp, &tmp2, &third64);
    UPPER(tmp) = 0;
    LOWER(tmp) = LOWER(products[3][1]);
    UPPER(tmp2) = 0;
    LOWER(tmp2) = UPPER(products[3][2]);
    add128(&tmp, &tmp2, &second64);
    UPPER(tmp) = 0;
    LOWER(tmp) = LOWER(products[3][0]);
    UPPER(tmp2) = 0;
    LOWER(tmp2) = UPPER(products[3][1]);
    add128(&tmp, &tmp2, &first64);

    UPPER(tmp) = 0;
    LOWER(tmp) = LOWER(products[2][3]);
    add128(&tmp, &third64, &third64);
    UPPER(tmp) = 0;
    LOWER(tmp) = LOWER(products[2][2]);
    add128(&tmp, &second64, &tmp2);
    UPPER(tmp) = 0;
    LOWER(tmp) = UPPER(products[2][3]);
    add128(&tmp, &tmp2, &second64);
    UPPER(tmp) = 0;
    LOWER(tmp) = LOWER(products[2][1]);
    add128(&tmp, &first64, &tmp2);
    UPPER(tmp) = 0;
    LOWER(tmp) = UPPER(products[2][2]);
    add128(&tmp, &tmp2, &first64);

    UPPER(tmp) = 0;
    LOWER(tmp) = LOWER(products[1][3]);
    add128(&tmp, &second64, &second64);
    UPPER(tmp) = 0;
    LOWER(tmp) = LOWER(products[1][2]);
    add128(&tmp, &first64, &tmp2);
    UPPER(tmp) = 0;
    LOWER(tmp) = UPPER(products[1][3]);
    add128(&tmp, &tmp2, &first64);

    UPPER(tmp) = 0;
    LOWER(tmp) = LOWER(products[0][3]);
    add128(&tmp, &first64, &first64);

    uint256_t target1, target2;
    clear256(&target1);
    UPPER(UPPER(target1)) = LOWER(first64);
    clear256(&target2);
    LOWER(UPPER(target2)) = UPPER(third64);
    UPPER(LOWER(target2)) = LOWER(third64);
    add256(&target1, &target2, target);
    clear256(&target1);
    copy128(&UPPER(target1), &second64);
    add256(&target1, target, &target2);
    clear256(&target1);
    copy128(&LOWER(target1), &fourth64);
    add256(&target1, &target2, target);
}

void mul512(const uint512_t *number1, const uint512_t *number2, uint512_t *target) {
    uint256_t top[4];
    uint256_t bottom[4];
    clear128(&UPPER(top[0]));
    copy128(&LOWER(top[0]), &UPPER(UPPER_P(number1)));
    clear128(&UPPER(top[1]));
    copy128(&LOWER(top[1]), &LOWER(UPPER_P(number1)));
    clear128(&UPPER(top[2]));
    copy128(&LOWER(top[2]), &UPPER(LOWER_P(number1)));
    clear128(&UPPER(top[3]));
    copy128(&LOWER(top[3]), &LOWER(LOWER_P(number1)));
    clear128(&UPPER(bottom[0]));
    copy128(&LOWER(bottom[0]), &UPPER(UPPER_P(number2)));
    clear128(&UPPER(bottom[1]));
    copy128(&LOWER(bottom[1]), &LOWER(UPPER_P(number2)));
    clear128(&UPPER(bottom[2]));
    copy128(&LOWER(bottom[2]), &UPPER(LOWER_P(number2)));
    clear128(&UPPER(bottom[3]));
    copy128(&LOWER(bottom[3]), &LOWER(LOWER_P(number2)));

    uint256_t products[4][4];
    for (uint8_t y = 4; y --> 0;) {
        for (uint8_t x = 4; x --> 0;) {
            mul256(&top[x], &bottom[y], &products[x][y]);
        }
    }

    uint256_t tmp, tmp2, fourth64, third64, second64, first64;
    clear128(&UPPER(fourth64));
    copy128(&LOWER(fourth64), &LOWER(products[3][3]));
    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &LOWER(products[3][2]));
    clear128(&UPPER(tmp2));
    copy128(&LOWER(tmp2), &UPPER(products[3][3]));
    add256(&tmp, &tmp2, &third64);
    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &LOWER(products[3][1]));
    clear128(&UPPER(tmp2));
    copy128(&LOWER(tmp2), &UPPER(products[3][2]));
    add256(&tmp, &tmp2, &second64);
    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &LOWER(products[3][0]));
    clear128(&UPPER(tmp2));
    copy128(&LOWER(tmp2), &UPPER(products[3][1]));
    add256(&tmp, &tmp2, &first64);

    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &LOWER(products[2][3]));
    add256(&tmp, &third64, &third64);
    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &LOWER(products[2][2]));
    add256(&tmp, &second64, &tmp2);
    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &UPPER(products[2][3]));
    add256(&tmp, &tmp2, &second64);
    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &LOWER(products[2][1]));
    add256(&tmp, &first64, &tmp2);
    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &UPPER(products[2][2]));
    add256(&tmp, &tmp2, &first64);

    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &LOWER(products[1][3]));
    add256(&tmp, &second64, &second64);
    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &LOWER(products[1][2]));
    add256(&tmp, &first64, &tmp2);
    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &UPPER(products[1][3]));
    add256(&tmp, &tmp2, &first64);

    clear128(&UPPER(tmp));
    copy128(&LOWER(tmp), &LOWER(products[0][3]));
    add256(&tmp, &first64, &first64);

    uint512_t target1, target2;
    clear512(&target1);
    copy128(&UPPER(UPPER(target1)), &LOWER(first64));
    clear512(&target2);
    copy128(&LOWER(UPPER(target2)), &UPPER(third64));
    copy128(&UPPER(LOWER(target2)), &LOWER(third64));
    add512(&target1, &target2, target);
    clear512(&target1);
    copy256(&UPPER(target1), &second64);
    add512(&target1, target, &target2);
    clear512(&target1);
    copy256(&LOWER(target1), &fourth64);
    add512(&target1, &target2, target);
}

void exp256(const uint256_t *base, const uint256_t *power, uint256_t *target) {
    uint256_t remaining, result;
    copy256(&remaining, power);
    UPPER(UPPER(result)) = 0;
    LOWER(UPPER(result)) = 0;
    UPPER(LOWER(result)) = 0;
    LOWER(LOWER(result)) = 1;
    uint256_t multiplier;
    copy256(&multiplier, base);
    while (!zero256(&remaining)) {
        if (LOWER(LOWER(remaining)) & 1) {
            mul256(&result, &multiplier, &result);
        }
        mul256(&multiplier, &multiplier, &multiplier);
        shiftr256(&remaining, 1, &remaining);
    }
    copy256(target, &result);
}

void signextend256(const uint256_t *base, uint8_t signBit, uint256_t *target) {
    shiftl256(base, 256 - signBit, target);
    shiftar256(target, 256 - signBit, target);
}

void divmod128(const uint128_t *l, const uint128_t *r, uint128_t *retDiv,
               uint128_t *retMod) {
    uint128_t copyd, adder, resDiv, resMod;
    uint128_t one;
    UPPER(one) = 0;
    LOWER(one) = 1;
    uint32_t diffBits = bits128(l) - bits128(r);
    clear128(&resDiv);
    copy128(&resMod, l);
    if (gt128(r, l)) {
        copy128(retMod, l);
        clear128(retDiv);
    } else {
        shiftl128(r, diffBits, &copyd);
        shiftl128(&one, diffBits, &adder);
        if (gt128(&copyd, &resMod)) {
            shiftr128(&copyd, 1, &copyd);
            shiftr128(&adder, 1, &adder);
        }
        while (gte128(&resMod, r)) {
            if (gte128(&resMod, &copyd)) {
                minus128(&resMod, &copyd, &resMod);
                or128(&resDiv, &adder, &resDiv);
            }
            shiftr128(&copyd, 1, &copyd);
            shiftr128(&adder, 1, &adder);
        }
        copy128(retDiv, &resDiv);
        copy128(retMod, &resMod);
    }
}

void divmod256(const uint256_t *l, const uint256_t *r, uint256_t *retDiv, uint256_t *retMod) {
    if (gt256(r, l)) {
        copy256(retMod, l);
        clear256(retDiv);
    } else {
        uint256_t copyd, adder, resDiv, resMod;
        uint256_t one;
        clear256(&one);
        LOWER(LOWER(one)) = 1;
        uint32_t diffBits = bits256(l) - bits256(r);
        copy256(&resMod, l);
        shiftl256(r, diffBits, &copyd);
        shiftl256(&one, diffBits, &adder);
        if (gt256(&copyd, &resMod)) {
            shiftr256(&copyd, 1, &copyd);
            shiftr256(&adder, 1, &adder);
        }
        clear256(&resDiv);
        while (gte256(&resMod, r)) {
            if (gte256(&resMod, &copyd)) {
                minus256(&resMod, &copyd, &resMod);
                or256(&resDiv, &adder, &resDiv);
            }
            shiftr256(&copyd, 1, &copyd);
            shiftr256(&adder, 1, &adder);
        }
        copy256(retDiv, &resDiv);
        copy256(retMod, &resMod);
    }
}

void divmod512(const uint512_t *l, const uint512_t *r, uint512_t *retDiv, uint512_t *retMod) {
    if (gt512(r, l)) {
        copy512(retMod, l);
        clear512(retDiv);
    } else {
        uint512_t copyd, adder, resDiv, resMod;
        uint512_t one;
        clear512(&one);
        LOWER(LOWER(LOWER(one))) = 1;
        uint32_t diffBits = bits512(l) - bits512(r);
        copy512(&resMod, l);
        shiftl512(r, diffBits, &copyd);
        shiftl512(&one, diffBits, &adder);
        if (gt512(&copyd, &resMod)) {
            shiftr512(&copyd, 1, &copyd);
            shiftr512(&adder, 1, &adder);
        }
        clear512(&resDiv);
        while (gte512(&resMod, r)) {
            if (gte512(&resMod, &copyd)) {
                minus512(&resMod, &copyd, &resMod);
                or512(&resDiv, &adder, &resDiv);
            }
            shiftr512(&copyd, 1, &copyd);
            shiftr512(&adder, 1, &adder);
        }
        copy512(retDiv, &resDiv);
        copy512(retMod, &resMod);
    }
}

void addmod256(const uint256_t *number1, const uint256_t *number2, const uint256_t *divisor, uint256_t *target) {
    uint512_t one, two, sum, three, result;
    clear256(&UPPER_P(&one));
    clear256(&UPPER_P(&two));
    clear256(&UPPER_P(&three));
    copy256(&LOWER_P(&one), number1);
    copy256(&LOWER_P(&two), number2);
    copy256(&LOWER_P(&three), divisor);
    add512(&one, &two, &sum);
    if (zero256(&UPPER(sum))) {
        divmod256(&LOWER(sum), divisor, &LOWER(one), target);
    } else {
        copy256(&LOWER(two), divisor);
        divmod512(&sum, &two, &one, &result);
        copy256(target, &LOWER(result));
    }
}

void mulmod256(const uint256_t *number1, const uint256_t *number2, const uint256_t *divisor, uint256_t *target) {
    uint512_t one, two, product, three, result;
    clear256(&UPPER_P(&one));
    clear256(&UPPER_P(&two));
    clear256(&UPPER_P(&three));
    copy256(&LOWER_P(&one), number1);
    copy256(&LOWER_P(&two), number2);
    copy256(&LOWER_P(&three), divisor);
    mul512(&one, &two, &product);
    if (zero256(&UPPER(product))) {
        divmod256(&LOWER(product), divisor, &LOWER(one), target);
    } else {
        copy256(&LOWER(two), divisor);
        divmod512(&product, &two, &one, &result);
        copy256(target, &LOWER(result));
    }
}


static inline void reverseString(char *str, uint32_t length) {
    uint32_t i, j;
    for (i = 0, j = length - 1; i < j; i++, j--) {
        uint8_t c;
        c = str[i];
        str[i] = str[j];
        str[j] = c;
    }
}

bool tostring128(const uint128_t *number, uint32_t baseParam, char *out,
                 uint32_t outLength) {
    if ((baseParam < 2) || (baseParam > 36)) {
        return false;
    }
    uint128_t rDiv;
    uint128_t rMod;
    uint128_t base;
    copy128(&rDiv, number);
    clear128(&base);
    LOWER(base) = baseParam;
    uint32_t offset = 0;
    do {
        if (offset > (outLength - 1)) {
            return false;
        }
        divmod128(&rDiv, &base, &rDiv, &rMod);
        out[offset++] = HEXDIGITS[LOWER(rMod)];
    } while (!zero128(&rDiv));
    out[offset] = '\0';
    reverseString(out, offset);
    return true;
}

bool tostring256(const uint256_t *number, uint32_t baseParam, char *out,
                 uint32_t outLength) {
    if ((baseParam < 2) || (baseParam > 36)) {
        return false;
    }
    uint256_t rDiv;
    uint256_t rMod;
    uint256_t base;
    copy256(&rDiv, number);
    clear256(&base);
    UPPER(LOWER(base)) = 0;
    LOWER(LOWER(base)) = baseParam;
    uint32_t offset = 0;
    do {
        if (offset > (outLength - 1)) {
            return false;
        }
        divmod256(&rDiv, &base, &rDiv, &rMod);
        out[offset++] = HEXDIGITS[LOWER(LOWER(rMod))];
    } while (!zero256(&rDiv));
    out[offset] = '\0';
    reverseString(out, offset);
    return true;
}
