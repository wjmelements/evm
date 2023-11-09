#include "evm.h"
#include "ops.h"
#include "vector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


static inline void DataCopy(data_t *dst, const data_t *src) {
    dst->content = malloc(src->size);
    memcpy(dst->content, src->content, dst->size = src->size);
}
#define BalanceCopy(dst, src) memcpy(dst, src, 12)
static inline void BalanceAdd(val_t to, val_t amount) {
    uint32_t carry = 0;
    uint8_t i = 3;
    while (i --> 0) {
        uint32_t result = carry + to[i] + amount[i];
        if (result < amount[i]) {
            carry = 1;
        } else {
            carry = 0;
        }
        to[i] = result;
    }
}

typedef uint256_t evmStack_t[1024];

typedef struct account {
    address_t address;
    val_t balance;
    data_t code;
    uint64_t nonce;
} account_t;

VECTOR(uint8, memory);
typedef struct {
    evmStack_t bottom;
    uint256_t *top;
    account_t *account;
    address_t caller;
    data_t code;
    val_t callValue;
    data_t returnData;
    memory_t memory;
    data_t callData;
} context_t;

// for debugging
static inline void dumpStack(context_t *context) {
    uint256_t *pos = context->top;
    char *buf = calloc(67, 1);
    while (--pos >= context->bottom) {
        tostring256(pos, 16, buf, 67);
        fprintf(stderr, "%lu: %s [%llx %llx %llx %llx]\n", pos - context->bottom, buf, UPPER(UPPER_P(pos)), LOWER(UPPER_P(pos)), UPPER(LOWER_P(pos)), LOWER(LOWER_P(pos)));
    }
    free(buf);
}

static inline void ensureMemory(context_t *callContext, uint64_t capacity) {
    // TODO gas from memory expansion
    memory_ensure(&callContext->memory, capacity);
}

typedef struct {
    context_t bottom[1024];
    context_t *next;
} callstack_t;

callstack_t callstack;

static account_t accounts[1024];
static account_t *emptyAccount;
static account_t const *dnfAccount = &accounts[1024];

void evmInit() {
    callstack.next = callstack.bottom;
    emptyAccount = accounts;
}

void evmFinalize() {
}

static account_t *getAccount(const address_t address) {
    account_t *result = accounts;
    while (result < emptyAccount && !AddressEqual(result->address, address)) result++;
    if (result == emptyAccount) {
        emptyAccount++;
        AddressCopy(result->address, address);
        result->code.size = 0;
        result->nonce = 0;
    }
    return result;
}

static account_t *createNewAccount(address_t from, uint64_t nonce) {
    account_t *result = emptyAccount++;
    address_t expected; // TODO
    AddressCopy(result->address, expected);
    bzero(result->balance, 10);
    return result;
}

static result_t doCall(context_t *callContext) {
    result_t result;
    uint8_t pushSize;
    uint64_t pc = 0;
    clear256(&result.status);
    uint256_t *src256;
    uint256_t *dst256;
    uint8_t buffer[32];
    while (1) {
        op_t op = callContext->code.content[pc++];
        //fprintf(stderr, "op %s\n", opString[op]);
        if (callContext->top < callContext->bottom + argCount[op]) {
            // stack underflow
            fprintf(stderr, "Stack underflow at pc %llu op %s stack depth %lu\n", pc - 1, opString[op], callContext->top - callContext->bottom);
            result.returnData.size = 0;
            return result;
        }
        switch (op) {
            case PUSH0:
            case PUSH1:
            case PUSH2:
            case PUSH3:
            case PUSH4:
            case PUSH5:
            case PUSH6:
            case PUSH7:
            case PUSH8:
            case PUSH9:
            case PUSH10:
            case PUSH11:
            case PUSH12:
            case PUSH13:
            case PUSH14:
            case PUSH15:
            case PUSH16:
            case PUSH17:
            case PUSH18:
            case PUSH19:
            case PUSH20:
            case PUSH21:
            case PUSH22:
            case PUSH23:
            case PUSH24:
            case PUSH25:
            case PUSH26:
            case PUSH27:
            case PUSH28:
            case PUSH29:
            case PUSH30:
            case PUSH31:
            case PUSH32:
                pushSize = op - PUSH0;
                dst256 = callContext->top++;
                bzero(&buffer, 32 - pushSize);
                memcpy(buffer + 32 - pushSize, callContext->code.content + pc, pushSize);
                readu256BE(buffer, dst256);
                pc += pushSize;
                break;
            case DUP1:
            case DUP2:
            case DUP3:
            case DUP4:
            case DUP5:
            case DUP6:
            case DUP7:
            case DUP8:
            case DUP9:
            case DUP10:
            case DUP11:
            case DUP12:
            case DUP13:
            case DUP14:
            case DUP15:
            case DUP16:
                src256 = callContext->top - (op - PUSH32);
                dst256 = (callContext->top++);
                copy256(dst256, src256);
                break;
            case POP:
                callContext->top--;
                break;
            default:
                fprintf(stderr, "Unsupported opcode %u\n", op);
                result.returnData.size = 0;
                return result;
            case STOP:
                LOWER(LOWER(result.status)) = 1;
                return result;
            case MSTORE:
                ensureMemory(callContext, 32 + LOWER(LOWER_P((callContext->top - 1))));
                uint8_t *loc = (callContext->memory.uint8s + LOWER(LOWER_P((callContext->top - 1))));
                callContext->top -= 2;
                dumpu256BE(callContext->top, loc);
                break;
            case MLOAD:
                ensureMemory(callContext, 32 + LOWER(LOWER_P((callContext->top - 1))));
                readu256BE(callContext->memory.uint8s + LOWER(LOWER_P((callContext->top - 1))), callContext->top-2);
                callContext->top--;
                break;
            case RETURN:
                LOWER(LOWER(result.status)) = 1;
                // intentional fallthrough
            case REVERT:
                ensureMemory(callContext, LOWER(LOWER_P((callContext->top - 1))) + LOWER(LOWER_P((callContext->top - 2))));
                result.returnData.content = callContext->memory.uint8s + LOWER(LOWER_P((--callContext->top)));
                result.returnData.size = LOWER(LOWER_P((--callContext->top)));
                return result;
        }
    }
}

result_t evmCall(address_t from, uint64_t gas, address_t to, val_t value, data_t input) {
    context_t *callContext = callstack.next;
    callstack.next += 1;

    callContext->top = callContext->bottom;
    BalanceCopy(callContext->callValue, value);
    AddressCopy(callContext->caller, from);
    callContext->account = getAccount(to);
    callContext->code = callContext->account->code;
    callContext->callData = input;
    BalanceAdd(callContext->account->balance, value);
    memory_init(&callContext->memory, 0);

    result_t result = doCall(callContext);

    return result;
}

result_t evmCreate(address_t from, uint64_t gas, val_t value, data_t input) {
    context_t *callContext = callstack.next;
    callstack.next += 1;

    callContext->top = callContext->bottom;
    BalanceCopy(callContext->callValue, value);
    AddressCopy(callContext->caller, from);
    account_t *fromAccount = getAccount(from);
    callContext->account = createNewAccount(from, fromAccount->nonce);
    callContext->code = input;
    callContext->callData.size = 0;
    memory_init(&callContext->memory, 0);

    result_t result = doCall(callContext);

    return result;
}
