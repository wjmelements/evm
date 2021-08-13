#include "disassemble.h"
#include "hex.h"

#include <stdint.h>
#include <stdio.h>

statement_stack_t stack;

void disassembleInit() {
    statement_stack_init(&stack, 8);
}
static void disassembleWaste(const char **iter) {
    while (**iter && !isHex(**iter)) {
        (*iter)++;
    }
    if (**iter == '0' && *(*iter + 1) == 'x') {
        (*iter) += 2;
        disassembleWaste(iter);
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
    char *str = (char *)calloc(bufLength, 1);
    int strLength = snprintf(str, bufLength, "%llu", value);
    statement_t pushDec = {
        strLength,
        bufLength,
        str
    };
    statement_stack_append(&stack, pushDec);
}
static void disassemblePushHex(op_t op, uint8_t pushlen, const char **iter) {
    size_t strLength = 2 * pushlen + 2;
    char *str = (char *)calloc(strLength, 1);
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
    statement_t pushHex = {
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
    uint8_t bufLen = 15;
    char *str = calloc(bufLen, 1);
    char *end = stpncpy(str, opString[op], bufLen);
    uint8_t strLen = end - str;
    statement_t op_statement = {
        strLen,
        bufLen,
        str
    };
    int argc = argCount[op];
    if (argc && argc <= stack.num_statements) {
        statement_append(&op_statement, '(');
        while (argc--) {
            statement_t arg = statement_stack_pop(&stack);
            for (uint32_t i = 0; i < arg.num_schars; i++) {
                statement_append(&op_statement,arg.schars[i]);
            }
            free(arg.schars);
            if (argc) {
                statement_append(&op_statement,',');
            }
        }
        statement_append(&op_statement, ')');
    }
    statement_stack_append(&stack, op_statement);
}
int disassembleValid(const char **iter) {
    disassembleWaste(iter);
    return **iter;
}
void disassembleFinalize() {
    for (size_t i = 0; i < stack.num_statements; i++) {
        puts(stack.statements[i].schars);
    }
}
