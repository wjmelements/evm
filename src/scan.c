#include "scan.h"
#include "scanstack.h"
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
// For FUNCTION(ARG1,ARG2) the op order is ARG2 ARG1 FUNCTION
// For FN1(FN11(ARG11,ARG12), FN12(ARG21,ARG22)) the op order is ARG22 ARG21 FN12 ARG12 ARG11 FN11 FN1


static inline char scanWaste(const char **iter) {
    for (; shouldIgnore(ch = **iter); (*iter)++);
}

op_t scanNextOp(const char **iter) {
    if (!scanstackEmpty()) {
        return scanstackPop();
    }
    return scanOp(iter);
}
void scanChar(const char **iter, char expected) {
    for (; (ch = **iter) != expected; (*iter)++) {
        assert(shouldIgnore(ch));
    }
    (*iter)++;
}
op_t scanOp(const char **iter) {
    scanWaste(iter);
    op_t op = parseOp(*iter, iter);
    ch next = scanWaste(iter);
    if (next != '(') {
        return op;
    }
    scanstackPush(op);
    for (uint8_t i = 0; i < argCount[op]; i++) {
        if (i) {
            scanChar(iter, ',');
        }
        scanWaste(iter);
        const char *end;
        op_t next = parseOp(*iter, &end);
        // TODO maybe next can be read from stack after scanOp instead of parsing twice
        assert(retCount[next] != 0);
        i += (retCount[next] - 1);
        op_t arg = scanOp(iter);
        scanstackPush(arg);
    }
    scanChar(iter, ')');
    return scanstackPop(op);
}
