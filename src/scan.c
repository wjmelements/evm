#include "alpha.h"
#include "scan.h"
#include "labelQueue.h"
#include "scanstack.h"
#include "dec.h"
#include "hex.h"
#include <assert.h>
#include <string.h>

static inline int shouldIgnore(char ch) {
    return ch != '('
        && ch != ')'
        && ch != ','
        && ch != ':'
        && ch != '/'
        && ch != '-'
        && (ch < '0' || ch > '9')
        && (ch < 'A' || ch > 'Z')
        && (ch < 'a' || ch > 'z')
    ;
}

static uint32_t programCounter;

void scanInit() {
    programCounter = (uint32_t)-1;
    labelQueueInit();
}

int scanValid(const char **iter) {
    return **iter || !scanstackEmpty();
}

int isHexConstantPrefix(const char *iter) {
    return *(uint16_t *)iter == 'x0';
}

int isConstant(const char *iter) {
    return isDecimal(*iter);  
}

op_t parseHex(const char **iter) {
    const char *start = *iter;
    while (isHex(**iter)) ++(*iter);
    uint8_t words = 0;
    const char *end = *iter;
    while (end > start) {
        words++;
        if (end - start >= 2) {
            end -= 2;
            scanstackPush((op_t)hexString16ToUint8(end));
        } else {
            --end;
            scanstackPush((op_t)hexString8ToUint8(*end));
        }
    }
    assert(words <= 32);
    return (op_t)((PUSH1 - 1) + words);
}

static inline uint64_t hi32(uint64_t in) {
    return in >> 32;
}

static inline uint64_t lo32(uint64_t in) {
    return in & 0xffffffff;
}

op_t parseDecimal(const char **iter, int negative) {
    uint64_t words[4] = {0,0,0,0};
    while (isDecimal(**iter)) {
        // multiply number by 10
        for (uint8_t i = 4; i --> 0;) {
            // long multiplication
            uint64_t s0, s1, s2, s3;

            uint64_t x = lo32(words[i]) * 10;
            s0 = lo32(x);

            x = hi32(words[i]) * 10 + hi32(x);
            s1 = lo32(x);
            s2 = hi32(x);

            x = s1;
            s1 = lo32(x);

            x = s2 + hi32(x);
            s2 = lo32(x);
            s3 = hi32(x);

            words[i] = s1 << 32 | s0;
            uint64_t carry = s3 << 32 | s2;
            // add carry
            for (uint8_t j = i + 1; j < 4; j++) {
                words[j] += carry;
                if (carry > words[j]) {
                    carry = 1;
                } else break;
            }
        }
        uint8_t digit = *((*iter)++) - '0';
        // add digit
        for (uint8_t i = 0; i < 4; i++) {
            words[i] += digit;
            if (digit > words[i]) {
                digit = 1;
            } else break;
        }
    }
    if (negative) {
        // negate via -x = ~x + 1
        for (uint8_t i = 4; i --> 0;) {
            words[i] = ~words[i];
        }
        // add 1 with carry
        for (uint8_t i = 0; i < 4; i++) {
            words[i]++;
            if (words[i]) {
                break;
            }
        }
    }
    uint8_t start = 0;
    // determine start
    for (uint8_t i = 4; i --> 0;) {
        for (uint8_t j = 8; j --> 0;) {
            uint64_t shift = j * 8;
            uint8_t byte = (words[i] & (0xffllu << shift)) >> shift;
            if (byte) {
                start = i * 8 + j;
                i = 0; // break outer
                break;
            }
        }
    }
    for (uint8_t i = 0; i < 4; i++) {
        for (uint8_t j = 0; j < 8; j++) {
            if (i * 8 + j > start) {
                break;
            }
            uint64_t shift = j * 8;
            uint8_t byte = (words[i] & (0xffllu << shift)) >> shift;
            scanstackPush(byte);
        }
    }
    return (op_t) PUSH1 + start;
}


int parseNegative(const char **iter) {
    if (**iter == '-') {
        (*iter)++;
        return 1;
    }
    return 0;
}

op_t parseConstant(const char **iter) {
    if (isHexConstantPrefix(*iter)) {
        (*iter) += 2;
        return parseHex(iter);
    } else {
        int negative = 0;
        while (parseNegative(iter)) {
            negative = !negative;
        }
        return parseDecimal(iter, negative);
    }
}

// For FUNCTION(ARG1,ARG2) the op order is ARG2 ARG1 FUNCTION
// For FN1(FN11(ARG11,ARG12), FN12(ARG21,ARG22)) the op order is ARG22 ARG21 FN12 ARG12 ARG11 FN11 FN1


void scanChar(const char **iter, char expected) {
    for (char ch; (ch = **iter) != expected; (*iter)++) {
        if (!shouldIgnore(ch)) {
            fprintf(stderr, "When seeking %c found unexpected character %c, before: %s", expected, ch, *iter);
            assert(expected == ch);
        }
    }
    (*iter)++;
}

static void scanComment(const char **iter) {
    for (char ch; (ch = **iter) != '\n'; (*iter)++);
    (*iter)++;
}

static inline char scanWaste(const char **iter) {
    char ch;
    for (; shouldIgnore(ch = **iter) && ch; (*iter)++);
    if (ch == '/') {
        scanComment(iter);
        return scanWaste(iter);
    }
    return ch;
}

static void scanLabel(const char **iter) {
    const char *start = *iter;
    for (char ch; isLowerCase(**iter); (*iter)++);
    const char *end = *iter;
    char next = scanWaste(iter);
    if (next == ':') {
        (*iter)++;
        scanstackPushLabel(start, end - start, JUMPDEST);
    } else {
        scanstackPushLabel(start, end - start, STOP);
        scanstackPush(PUSH1);
    }
    scanWaste(iter);
}

static void scanOp(const char **iter) {
    scanWaste(iter);
    if (isConstant(*iter)) {
        op_t op = parseConstant(iter);
        scanWaste(iter);
        scanstackPush(op);
        return;
    } else if (isLowerCase(**iter)) {
        scanLabel(iter);
        return;
    }
    op_t op = parseOp(*iter, iter);
    char next = scanWaste(iter);
    scanstackPush(op);
    if (next != '(') {
        return;
    }
    scanChar(iter, '(');
    for (uint8_t i = 0; i < argCount[op]; i++) {
        if (i) {
            scanWaste(iter);
            scanChar(iter, ',');
        }
        scanWaste(iter);
        if (isConstant(*iter)) {
            scanstackPush(parseConstant(iter));
        } else if (isLowerCase(**iter)) {
            scanLabel(iter);
        } else {
            const char *end;
            op_t next = parseOp(*iter, &end);
            // TODO maybe next can be read from stack after scanOp instead of parsing twice
            assert(retCount[next] != 0);
            i += (retCount[next] - 1);
            scanOp(iter);
        }
    }
    scanWaste(iter);
    scanChar(iter, ')');
    scanWaste(iter);
}

op_t scanNextOp(const char **iter) {
    jump_t jump;
    jump.len = 1;
    programCounter++;
    jump.programCounter = programCounter;
    if (!scanstackEmpty()) {
        if (scanstackTopLabel(&jump.label)) {
            op_t type = scanstackPop();
            if (type == JUMPDEST) {
                registerLabel(jump);
            } else {
                labelQueuePush(jump);
            }
            return type;
        } else return scanstackPop();
    }
    scanOp(iter);
    if (scanstackTopLabel(&jump.label)) {
        op_t type = scanstackPop();
        if (type == JUMPDEST) {
            registerLabel(jump);
        } else {
            labelQueuePush(jump);
        }
        return type;
    } else return scanstackPop();
}


void shiftProgram(op_t* begin, uint32_t *programLength, uint32_t offset, uint32_t amount) {
    memmove(begin + offset + amount, begin + offset, *programLength - offset);
    *programLength += amount;
}

void scanFinalize(op_t *begin, uint32_t *programLength) {
    // set label indices
    node_t *node = head;
    while (node) {
        node->jump.labelIndex = labelCount;
        for (uint32_t i = 0; i < labelCount; i++) {
            if (labels[i].length != node->jump.label.length) {
                continue;
            }
            if (0 == strncmp(labels[i].start, node->jump.label.start, labels[i].length)) {
                node->jump.labelIndex = i;
                break;
            }
        }
        assert(node->jump.labelIndex != labelCount);
        node = node->next;
    }

    // find first label that needs larger len
    uint32_t labelIndex = firstLabelAfter(256);
    // loop through jumps, extending jump.len
    node = head;
    int again;
    do {
        again = 0;
        while (node) {
            if (labelLocations[node->jump.labelIndex] > 255 && node->jump.len == 1) {
                again = 1;
                node->jump.len++;
                shiftProgram(begin, programLength, node->jump.programCounter, 1);
                // shift jump.programCounter for subsequent jumps
                node_t *follower = node->next;
                while (follower) {
                    follower->jump.programCounter++;
                    follower = follower->next;
                }
                // change jump locations
                for (uint32_t i = firstLabelAfter(node->jump.programCounter); i < labelCount; i++) {
                    labelLocations[i]++;
                }
            }
            node = node->next;
        }
    } while (again);

    while (!labelQueueEmpty()) {
        jump_t jump = labelQueuePop();
        uint32_t location = labelLocations[jump.labelIndex]; //getLabelLocation(jump.label);
        if (location > 0xffff) {
            fprintf(stderr, "Unsupported label location %u\n", location);
        }
        if (location > 0xff) {
            begin[jump.programCounter - 1]++; // PUSH1 -> PUSH2
            begin[jump.programCounter] = location / 256;
            begin[jump.programCounter + 1] = location % 256;
        } else {
            begin[jump.programCounter] = location;
        }
    }
}
