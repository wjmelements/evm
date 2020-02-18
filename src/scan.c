#include "scan.h"
#include "scanstack.h"
#include "hex.h"
#include <assert.h>

static inline int shouldIgnore(char ch) {
    return ch != '('
        && ch != ')'
        && ch != ','
        && (ch < '0' || ch > '9')
        && (ch < 'A' || ch > 'Z')
        && (ch < 'a' || ch > 'z')
    ;
}

void scanInit() {

}

int scanValid(const char **iter) {
    return **iter || !scanstackEmpty();
}

int isConstantPrefix(const char *iter) {
    return *(uint16_t *)iter == 'x0';
}

op_t parseConstant(const char **iter) {
    if (isConstantPrefix(*iter)) {
        (*iter) += 2;
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
        return (op_t)((PUSH1 - 1) + words);
    } else {
        // TODO decimal?
        fprintf(stderr, "Unexpected constant: %s", *iter);
        return 0;
    }
}

// For FUNCTION(ARG1,ARG2) the op order is ARG2 ARG1 FUNCTION
// For FN1(FN11(ARG11,ARG12), FN12(ARG21,ARG22)) the op order is ARG22 ARG21 FN12 ARG12 ARG11 FN11 FN1


static inline char scanWaste(const char **iter) {
    char ch;
    for (; shouldIgnore(ch = **iter) && ch; (*iter)++);
    return ch;
}

void scanChar(const char **iter, char expected) {
    for (char ch; (ch = **iter) != expected; (*iter)++) {
        if (!shouldIgnore(ch)) {
            fprintf(stderr, "When seeking %c found unexpected character %c, before: %s", expected, ch, *iter);
            //assert(expected == ch);
        }
    }
    (*iter)++;
}
op_t scanOp(const char **iter) {
    scanWaste(iter);
    if (isConstantPrefix(*iter)) {
        return parseConstant(iter);
    }
    op_t op = parseOp(*iter, iter);
    char next = scanWaste(iter);
    if (next != '(') {
        return op;
    }
    scanstackPush(op);
    scanChar(iter, '(');
    for (uint8_t i = 0; i < argCount[op]; i++) {
        if (i) {
            scanChar(iter, ',');
        }
        scanWaste(iter);
        if (isConstantPrefix(*iter)) {
            scanstackPush(parseConstant(iter));
        } else {
            const char *end;
            op_t next = parseOp(*iter, &end);
            // TODO maybe next can be read from stack after scanOp instead of parsing twice
            assert(retCount[next] != 0);
            i += (retCount[next] - 1);
            op_t arg = scanOp(iter);
            scanstackPush(arg);
        }
    }
    scanChar(iter, ')');
    scanWaste(iter);
    return scanstackPop();
}
op_t scanNextOp(const char **iter) {
    if (!scanstackEmpty()) {
        return scanstackPop();
    }
    return scanOp(iter);
}
