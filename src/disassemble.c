#include "disassemble.h"
#include "hex.h"

#include <stdint.h>
#include <stdio.h>

statement_stack_t stack;

void disassembleInit() {
    // TODO free strings
    statement_stack_trimTo(&stack, 0);
}
static void disassembleWaste(const char **iter) {
    while (**iter && !isHex(**iter)) {
        *iter += 1;
    }
}
static void disassemblePushDecimal(op_t op, uint8_t pushlen, const char **iter) {
    uint64_t value = 0;
    while (pushlen) {
        disassembleWaste(iter);
        value *= 16;
        value += hexString8ToUint8(*(*iter)++);
        disassembleWaste(iter);
        value *= 16;
        value += hexString8ToUint8(*(*iter)++);
        pushlen--;
    }
    size_t bufLength = 20;
    char *str = (char *)malloc(bufLength);
    int strLength = sprintf(str, "%llu", value);
    stack_statement_t pushDec = {
        strLength,
        bufLength,
        str
    };
    statement_stack_append(&stack, pushDec);
}
static void disassemblePushHex(op_t op, uint8_t pushlen, const char **iter) {
    size_t strLength = pushlen + 2;
    char *str = (char *)malloc(strLength);
    str[0] = '0';
    str[1] = 'x';
    uint8_t i = 2;
    while (pushlen) {
        disassembleWaste(iter);
        str[i++] = *(*iter)++;
        disassembleWaste(iter);
        str[i++] = *(*iter)++;
        pushlen--;
    }
    stack_statement_t pushHex = {
        strLength,
        strLength,
        str
    };
    statement_stack_append(&stack, pushHex);
}

static void disassemblePush(op_t op, const char **iter) {
    uint8_t pushlen = op - PUSH1 + 1;
    if (pushlen <= 8) {
        disassemblePushDecimal(op, pushlen, iter);
    } else {
        disassemblePushHex(op, pushlen, iter);
    }
}
void disassembleNextOp(const char **iter) {
    disassembleWaste(iter);
    op_t op = hexString16ToUint8(*iter);
    *iter += 2;
    if (op >= PUSH1 && op <= PUSH32) {
        disassemblePush(op, iter);
        return;
    }
}
int disassembleValid(const char **iter) {
    disassembleWaste(iter);
    return **iter;
}
void disassembleFinalize() {
    puts("disassembleFinalize");
    for (size_t i = 0; i < stack.num_statements; i++) {
        puts(stack.statements[i].schars);
    }
}
