#include "evm.h"
#include "vector.h"

#include <assert.h>
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

static inline bool BalanceSub(val_t from, val_t amount) {
    uint32_t borrow = 0;
    uint8_t i = 3;
    val_t result;
    while (i --> 0) {
        result[i] = from[i] - amount[i] - borrow;
        if (result[i] > from[i]) {
            borrow = 1;
        } else {
            borrow = 0;
        }
    }
    if (borrow) {
        return false;
    }
    from[0] = result[0];
    from[1] = result[1];
    from[2] = result[2];
    return true;
}

typedef uint256_t evmStack_t[1024];

typedef struct storage {
    uint256_t key;
    uint256_t value;
    uint256_t original;
    uint64_t warm;
    struct storage *next;
} storage_t;

typedef struct account {
    address_t address;
    val_t balance;
    data_t code;
    uint64_t nonce;
    uint64_t warm;
    storage_t *next;
} account_t;

static bool AccountDead(account_t *account) {
    return !(account->balance[0] || account->balance[1] || account->balance[2] || account->code.size || account->nonce);
}

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
    uint64_t gas;
    bool readonly;
} context_t;

stateChanges_t *getCurrentAccountStateChanges(result_t *result, context_t *context) {
    stateChanges_t **stateChanges = &result->stateChanges;
    while (*stateChanges != NULL) {
        if (AddressEqual(&(*stateChanges)->account, &context->account->address)) {
            return *stateChanges;
        }
        stateChanges = &(*stateChanges)->next;
    }
    // DNF
    *stateChanges = malloc(sizeof(stateChanges_t));
    AddressCopy(&(*stateChanges)->account, &context->account->address);
    return *stateChanges;
}

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

static inline void dumpCallData(context_t *context) {
    for (uint32_t i = 0; i < context->callData.size; i++) {
        fprintf(stderr, "%02x", context->callData.content[i]);
    }
    fputc('\n', stderr);
}

static inline void dumpMemory(memory_t *memory) {
    for (uint32_t i = 0; i < memory->num_uint8s; i++) {
        fprintf(stderr, "%02x", memory->uint8s[i]);
        if (i % 32 == 31) {
            fputc('\n', stderr);
        }
    }
    if (memory->num_uint8s % 32 == 0) {
        return;
    }
    for (uint32_t i = 32 - memory->num_uint8s % 32; i --> 0;) {
        fputs("00", stderr);
    }
    fputc('\n', stderr);
}

static uint64_t memoryGasCost(uint64_t capacity) {
    uint64_t words = (capacity + 31) >> 5;
    return words * G_MEM + words * words / G_QUADDIV;
}

static inline bool ensureMemory(context_t *callContext, uint64_t capacity) {
    // TODO gas from memory expansion
    memory_ensure(&callContext->memory, capacity);
    if (callContext->memory.num_uint8s < capacity) {
        uint64_t memoryGas = memoryGasCost(capacity) - memoryGasCost(callContext->memory.num_uint8s);
        callContext->memory.num_uint8s = capacity;
        if (memoryGas > callContext->gas) {
            return false;
        }
        callContext->gas -= memoryGas;
    }
    return true;
}

static inline void deductCalldataGas(context_t *callContext, const data_t *calldata, bool isCreate) {
    callContext->gas -= G_CALLDATAZERO * calldata->size;
    if (isCreate) {
        callContext->gas -= G_INITCODEWORD * ((calldata->size + 31) >> 5); // EIP 3860: 2 gas per word
    }
    for (size_t i = 0; i < calldata->size; i++) {
        if (calldata->content[i]) {
            callContext->gas -= G_CALLDATANONZERO;
        }
    }
}

typedef struct {
    context_t bottom[1024];
    context_t *next;
} callstack_t;

callstack_t callstack;

static account_t accounts[1024];
static account_t *emptyAccount;
static account_t const *dnfAccount = &accounts[1024];
static uint64_t evmIteration = 0;
static uint64_t refundCounter = 0;
static uint64_t debugFlags = 0;

void evmSetDebug(uint64_t flags) {
    debugFlags = flags;
}

#define SHOW_STACK (debugFlags & EVM_DEBUG_STACK)
#define SHOW_MEMORY (debugFlags & EVM_DEBUG_MEMORY)
#define SHOW_OPS (debugFlags & EVM_DEBUG_OPS)
#define SHOW_GAS (debugFlags & EVM_DEBUG_GAS)

void evmInit() {
    callstack.next = callstack.bottom;
    while (emptyAccount --> accounts) {
        storage_t *storage = emptyAccount->next;
        while (storage != NULL) {
            void *toFree = storage;
            storage = storage->next;
            free(toFree);
        }
        emptyAccount->next = NULL;
        emptyAccount->warm = 0;
        bzero(emptyAccount->address.address, 20);
        op_t *code = emptyAccount->code.content;
        if (code != NULL) {
            free(code);
        }
        emptyAccount->code.content = NULL;
    }
    emptyAccount = accounts;
    evmIteration++;
    refundCounter = 0;
}

void evmFinalize() {
}

static account_t *getAccount(const address_t address) {
    account_t *result = accounts;
    while (result < emptyAccount && !AddressEqual(&result->address, &address)) result++;
    if (result == emptyAccount) {
        emptyAccount++;
        AddressCopy(result->address, address);
        result->code.size = 0;
        result->nonce = 0;
        result->balance[0] = 0;
        result->balance[1] = 0;
        result->balance[2] = 0;
    }
    return result;
}

void evmMockBalance(address_t from, const val_t balance) {
    account_t *account = getAccount(from);
    account->balance[0] = balance[0];
    account->balance[1] = balance[1];
    account->balance[2] = balance[2];
}

void evmMockCode(address_t to, data_t code) {
    getAccount(to)->code = code;
}

void evmMockNonce(address_t to, uint64_t nonce) {
    getAccount(to)->nonce = nonce;
}

typedef struct hashResult {
    val_t top96;
    address_t bottom160;
} __attribute((__packed__)) addressHashResult_t;

static account_t *createNewAccount(account_t *from) {
    uint64_t nonce = from->nonce++;
    uint8_t inputBuffer[28];
    if (nonce < 1) {
        // d6 94 address20 80
        inputBuffer[0] = 0xd6;
        inputBuffer[22] = 0x80;
    } else if (nonce < 0x80) {
        // d6 94 address20 nonce1
        inputBuffer[0] = 0xd6;
        inputBuffer[22] = nonce;
    } else if (nonce < 0x100) {
        // d7 94 address20 81 nonce1
        inputBuffer[0] = 0xd7;
        inputBuffer[22] = 0x81;
        inputBuffer[23] = nonce;
    } else if (nonce < 0x10000) {
        // d8 94 address20 82 nonce2
        inputBuffer[0] = 0xd8;
        inputBuffer[22] = 0x82;
        inputBuffer[23] = nonce >> 8;
        inputBuffer[24] = nonce;
    } else if (nonce < 0x1000000) {
        // d9 94 address20 83 nonce3
        inputBuffer[0] = 0xd9;
        inputBuffer[22] = 0x83;
        inputBuffer[23] = nonce >> 16;
        inputBuffer[24] = nonce >> 8;
        inputBuffer[25] = nonce;
    } else if (nonce < 0x100000000) {
        // da 94 address20 84 nonce4
        inputBuffer[0] = 0xda;
        inputBuffer[22] = 0x84;
        inputBuffer[23] = nonce >> 24;
        inputBuffer[24] = nonce >> 16;
        inputBuffer[25] = nonce >> 8;
        inputBuffer[26] = nonce;
    } else {
        fprintf(stderr, "Unsupported nonce %llu\n", nonce);
        return NULL;
    }
    inputBuffer[1] = 0x94;
    memcpy(inputBuffer + 2, from->address.address, 20);
    addressHashResult_t hashResult;
    keccak_256((uint8_t *)&hashResult, sizeof(hashResult), inputBuffer, inputBuffer[0] - 0xbf);
    account_t *result = getAccount(hashResult.bottom160);
    result->warm = evmIteration;
    return result;
}

static account_t *warmAccount(context_t *callContext, const address_t address) {
    account_t *account = getAccount(address);
    if (account->warm != evmIteration) {
        uint64_t gasCost = G_COLD_ACCOUNT - G_ACCESS;
        if (callContext->gas < gasCost) {
            return NULL;
        }
        callContext->gas -= gasCost;
        account->warm = evmIteration;
    }
    return account;
}

static storage_t *getAccountStorage(account_t *account, const uint256_t *key) {
    storage_t **storage = &account->next;
    while (*storage != NULL) {
        if (equal256(&(*storage)->key, key)) {
            return *storage;
        }
        storage = &(*storage)->next;
    }
    *storage = calloc(1, sizeof(storage_t));
    copy256(&(*storage)->key, key);
    return *storage;
}

void evmMockStorage(address_t to, const uint256_t *key, const uint256_t *storedValue) {
    account_t *account = getAccount(to);
    storage_t *storage = getAccountStorage(account, key);
    copy256(&storage->value, storedValue);
}

// you might expect the marginal cost of warming a slot is constant but actually it is 100 cheaper if you do it in SLOAD.
static storage_t *warmStorage(context_t *callContext, uint256_t *key, uint64_t warmGasCost) {
    account_t *account = callContext->account;
    storage_t *storage = getAccountStorage(account, key);
    if (storage->warm != evmIteration) {
        if (callContext->gas < warmGasCost) {
            return NULL;
        }
        callContext->gas -= warmGasCost;
        storage->warm = evmIteration;
        copy256(&storage->original, &storage->value);
    }
    return storage;
}

static result_t evmStaticCall(address_t from, uint64_t gas, address_t to, data_t input);

static result_t doCall(context_t *callContext) {
    //dumpCallData(callContext);
    result_t result;
    result.stateChanges = NULL;
    clear256(&result.status);
    uint64_t pc = 0;
    uint8_t buffer[32];
    while (1) {
        op_t op;
        if (pc < callContext->code.size) {
            op = callContext->code.content[pc++];
        } else {
            op = STOP;
        }

        if (SHOW_STACK) {
            dumpStack(callContext);
        }
        if (SHOW_MEMORY) {
            dumpMemory(&callContext->memory);
        }
        if (SHOW_OPS) {
            if (SHOW_GAS) {
                fprintf(stderr, "gas %llu ", callContext->gas);
            }
            fprintf(stderr, "op %s\n", opString[op]);
        }
        if (callContext->top < callContext->bottom + argCount[op]) {
            // stack underflow
            fprintf(stderr, "Stack underflow at pc %llu op %s stack depth %lu\n", pc - 1, opString[op], callContext->top - callContext->bottom);
            callContext->gas = 0;
            result.returnData.size = 0;
            return result;
        }
#define OUT_OF_GAS \
            fprintf(stderr, "Out of gas at pc %llu op %s\n", pc - 1, opString[op]);\
            result.returnData.size = 0; \
            return result
        // Check staticcall
        switch (op) {
        case CALL:
            if (zero256((callContext->top-3))) {
                // CALL is permitted without CALLVALUE
                break;
            }
            // intentional fallthrough
        case LOG0:
        case LOG1:
        case LOG2:
        case LOG3:
        case LOG4:
        case CREATE:
        case CREATE2:
        case SELFDESTRUCT:
        case SSTORE:
            if (callContext->readonly) {
                fprintf(stderr, "Attempted %s inside STATICCALL\n", opString[op]);
                callContext->gas = 0;
                result.returnData.size = 0;
                return result;\
            }
        default:
            break;
        }
        if (callContext->gas < gasCost[op]) {
            OUT_OF_GAS;
        }
        callContext->gas -= gasCost[op];
        callContext->top += retCount[op] - argCount[op];
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
                ;
                uint8_t pushSize = op - PUSH0;
                bzero(buffer, 32 - pushSize);
                memcpy(buffer + 32 - pushSize, callContext->code.content + pc, pushSize);
                readu256BE(buffer, callContext->top - 1);
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
                copy256(callContext->top - 1, callContext->top - (op - PUSH31));
                break;
            case SWAP1:
            case SWAP2:
            case SWAP3:
            case SWAP4:
            case SWAP5:
            case SWAP6:
            case SWAP7:
            case SWAP8:
            case SWAP9:
            case SWAP10:
            case SWAP11:
            case SWAP12:
            case SWAP13:
            case SWAP14:
            case SWAP15:
            case SWAP16:
                memcpy(buffer, callContext->top - 1, 32);
                memcpy(callContext->top - 1, callContext->top - (op - DUP15), 32);
                memcpy(callContext->top - (op - DUP15), buffer, 32);
                break;
            case ADDRESS:
                AddressToUint256(callContext->top - 1, &callContext->account->address);
                break;
            case CALLER:
                AddressToUint256(callContext->top - 1, &callContext->caller);
                break;
            case POP:
                // intentional fallthrough
            case JUMPDEST:
                break;
            case ADD:
                add256(callContext->top, callContext->top - 1, callContext->top + 1);
                copy256(callContext->top - 1, callContext->top + 1);
                break;
            case SUB:
                minus256(callContext->top, callContext->top - 1, callContext->top + 1);
                copy256(callContext->top - 1, callContext->top + 1);
                break;
            case MUL:
                mul256(callContext->top, callContext->top - 1, callContext->top + 1);
                copy256(callContext->top - 1, callContext->top + 1);
                break;
            case DIV:
                divmod256(callContext->top, callContext->top - 1, callContext->top + 1, callContext->top + 2);
                copy256(callContext->top - 1, callContext->top + 1);
                break;
            case MOD:
                divmod256(callContext->top, callContext->top - 1, callContext->top + 2, callContext->top + 1);
                copy256(callContext->top - 1, callContext->top + 1);
                break;
            case XOR:
                xor256(callContext->top, callContext->top - 1, callContext->top - 1);
                break;
            case OR:
                or256(callContext->top, callContext->top - 1, callContext->top - 1);
                break;
            case AND:
                and256(callContext->top, callContext->top - 1, callContext->top - 1);
                break;
            case NOT:
                not256(callContext->top - 1, callContext->top - 1);
                break;
            case LT:
                LOWER(LOWER_P(callContext->top - 1)) = gte256(callContext->top - 1, callContext->top);
                bzero(callContext->top - 1, 24);
                break;
            case GT:
                LOWER(LOWER_P(callContext->top - 1)) = gt256(callContext->top, callContext->top - 1);
                bzero(callContext->top - 1, 24);
                break;
            case EQ:
                LOWER(LOWER_P(callContext->top - 1)) = equal256(callContext->top, callContext->top - 1);
                bzero(callContext->top - 1, 24);
                break;
            case ISZERO:
                LOWER(LOWER_P(callContext->top - 1)) = zero256(callContext->top - 1);
                bzero(callContext->top - 1, 24);
                break;
            case PC:
                bzero(callContext->top - 1, 24);
                LOWER(LOWER_P(callContext->top - 1)) = pc - 1;
                break;
            case JUMPI:
                if (zero256(callContext->top)) {
                    break;
                }
                // intentional fallthorugh
            case JUMP:
                pc = LOWER(LOWER_P(callContext->top + (op - JUMP)));
                if (pc >= callContext->code.size) {
                    fprintf(stderr, "JUMP out of bounds %llu >= %lu\n", pc, callContext->code.size);
                    result.returnData.size = 0;
                    callContext->gas = 0;
                    return result;
                }
                if (callContext->code.content[pc] != JUMPDEST) {
                    fprintf(stderr, "JUMP to invalid destination %llu (%s)\n", pc, opString[callContext->code.content[pc]]);
                    result.returnData.size = 0;
                    callContext->gas = 0;
                    return result;
                } // TODO static analysis for PUSH
                break;
            default:
                fprintf(stderr, "Unsupported opcode %u (%s)\n", op, opString[op]);
                result.returnData.size = 0;
                callContext->gas = 0;
                return result;
            case STOP:
                LOWER(LOWER(result.status)) = 1;
                result.returnData.size = 0;
                return result;
            case GAS:
                bzero(callContext->top - 1, 24);
                LOWER(LOWER_P(callContext->top - 1)) = callContext->gas;
                break;
            case RETURNDATASIZE:
                bzero(callContext->top - 1, 24);
                LOWER(LOWER_P(callContext->top - 1)) = callContext->returnData.size;
                break;
            case CALLDATASIZE:
                bzero(callContext->top - 1, 24);
                LOWER(LOWER_P(callContext->top - 1)) = callContext->callData.size;
                break;
            case EXTCODESIZE:
                {
                    account_t *account = warmAccount(callContext, AddressFromUint256(callContext->top - 1));
                    if (account == NULL) {
                        OUT_OF_GAS;
                    }
                    bzero(callContext->top - 1, 24);
                    LOWER(LOWER_P(callContext->top - 1)) = account->code.size;
                }
                break;
            case CODESIZE:
                bzero(callContext->top - 1, 24);
                LOWER(LOWER_P(callContext->top - 1)) = callContext->code.size;
                break;
            case MSIZE:
                clear256(callContext->top - 1);
                uint64_t scratch = callContext->memory.num_uint8s;
                if (scratch % 32) {
                    scratch += 32 - scratch % 32;
                }
                LOWER(LOWER_P(callContext->top - 1)) = scratch;
                break;
            case MSTORE:
                if (!ensureMemory(callContext, 32 + LOWER(LOWER_P(callContext->top + 1)))) {
                    OUT_OF_GAS;
                }
                uint8_t *loc = (callContext->memory.uint8s + LOWER(LOWER_P(callContext->top + 1)));
                dumpu256BE(callContext->top, loc);
                break;
            case MLOAD:
                if (UPPER(LOWER_P(callContext->top - 1)) || LOWER(UPPER_P(callContext->top - 1)) || UPPER(UPPER_P(callContext->top - 1))) {
                    OUT_OF_GAS;
                }
                if (!ensureMemory(callContext, 32 + LOWER(LOWER_P(callContext->top - 1)))) {
                    OUT_OF_GAS;
                }
                readu256BE(callContext->memory.uint8s + LOWER(LOWER_P(callContext->top - 1)), callContext->top - 1);
                break;
            case CALLDATALOAD:
                if (UPPER(LOWER_P(callContext->top - 1)) || LOWER(UPPER_P(callContext->top - 1)) || UPPER(UPPER_P(callContext->top - 1)) || LOWER(LOWER_P(callContext->top - 1)) >= callContext->callData.size) {
                    clear256(callContext->top - 1);
                    break;
                }
                // TODO handle intersecting calldatasize boundary
                readu256BE(callContext->callData.content + LOWER(LOWER_P(callContext->top - 1)), callContext->top - 1);
                break;
            case CALLDATACOPY:
            case EXTCODECOPY:
            case CODECOPY:
                {
                    const data_t *code;
                    if (op == EXTCODECOPY) {
                        account_t *account = warmAccount(callContext, AddressFromUint256(callContext->top + 3));
                        if (account == NULL) {
                            OUT_OF_GAS;
                        }
                        code = &account->code;
                    } else if (op == CALLDATACOPY) {
                        code = &callContext->callData;
                    } else {
                        code = &callContext->code;
                    }
                    uint64_t size = LOWER(LOWER_P(callContext->top));
                    uint64_t dst = LOWER(LOWER_P(callContext->top + 2));
                    if (
                        UPPER(LOWER_P(callContext->top + 2)) || LOWER(UPPER_P(callContext->top + 2)) || UPPER(UPPER_P(callContext->top + 2))
                        || (UPPER(LOWER_P(callContext->top)) || LOWER(UPPER_P(callContext->top)) || UPPER(UPPER_P(callContext->top)))
                        || dst + size < dst
                        || !ensureMemory(callContext, dst + size)
                    ) {
                        OUT_OF_GAS;
                    }
                    uint64_t words = (size + 31) / 32;
                    uint64_t gasCost = G_COPY * words;
                    if (gasCost > callContext->gas) {
                        OUT_OF_GAS;
                    }
                    callContext->gas -= gasCost;
                    uint64_t start = LOWER(LOWER_P(callContext->top + 1));
                    if (
                            UPPER(LOWER_P(callContext->top + 1)) || LOWER(UPPER_P(callContext->top + 1)) || UPPER(UPPER_P(callContext->top + 1))
                            || start > code->size
                        ) {
                        bzero(callContext->memory.uint8s + dst, size);
                    } else if (start + size > code->size) {
                        uint64_t copySize = code->size - start;
                        memcpy(callContext->memory.uint8s + dst, code->content + start, copySize);
                        bzero(callContext->memory.uint8s + dst + copySize, size - copySize);
                    } else {
                        memcpy(callContext->memory.uint8s + dst, code->content + start, size);
                    }
                }
                break;
            case SSTORE:
                {
                    if (callContext->gas <= G_CALLSTIPEND - G_ACCESS) {
                        OUT_OF_GAS;
                    }
                    uint64_t warmBefore = getAccountStorage(callContext->account, callContext->top + 1)->warm;
                    storage_t *storage = warmStorage(callContext, callContext->top + 1, G_COLD_STORAGE);
                    if (storage == NULL) {
                        OUT_OF_GAS;
                    }
                    // https://eips.ethereum.org/EIPS/eip-2200
                    if (!equal256(&storage->value, callContext->top)) {
                        if (equal256(&storage->value, &storage->original)) {
                            uint64_t gasCost;
                            if (zero256(&storage->original)) {
                                gasCost = G_SSET - G_ACCESS;
                            } else {
                                gasCost = G_SRESET - G_ACCESS - G_COLD_STORAGE;
                                if (zero256(callContext->top)) {
                                    refundCounter += R_CLEAR;
                                }
                            }
                            if (gasCost > callContext->gas) {
                                OUT_OF_GAS;
                            }
                            callContext->gas -= gasCost;
                        } else {
                            if (!zero256(&storage->original)) {
                                if (zero256(&storage->value)) {
                                    refundCounter -= R_CLEAR;
                                } else if (zero256(callContext->top)) {
                                    refundCounter += R_CLEAR;
                                }
                            }
                            if (equal256(&storage->original, callContext->top)) {
                                if (zero256(&storage->original)) {
                                    refundCounter += G_SSET - G_ACCESS;
                                } else {
                                    refundCounter += G_SRESET - G_ACCESS;
                                }
                            }
                        }
                    }
                    // track state changes in result in case of REVERT or exception
                    stateChanges_t *changes = getCurrentAccountStateChanges(&result, callContext);
                    storageChanges_t *change = malloc(sizeof(storageChanges_t));
                    copy256(&change->key, &storage->key);
                    copy256(&change->before, &storage->value);
                    copy256(&change->after, callContext->top);
                    change->warm = warmBefore;
                    change->prev = changes->storageChanges;
                    changes->storageChanges = change;
                    copy256(&storage->value, callContext->top);
                }
                break;
            case SLOAD:
                {
                    uint64_t warmBefore = getAccountStorage(callContext->account, callContext->top + 1)->warm;
                    storage_t *storage = warmStorage(callContext, callContext->top - 1, G_COLD_STORAGE - G_ACCESS);
                    if (storage == NULL) {
                        OUT_OF_GAS;
                    }
                    copy256(callContext->top - 1, &storage->value);
                    // track access list changes in result in case of REVERT or exception
                    stateChanges_t *changes = getCurrentAccountStateChanges(&result, callContext);
                    storageChanges_t *change = malloc(sizeof(storageChanges_t));
                    copy256(&change->key, &storage->key);
                    copy256(&change->before, &storage->value);
                    copy256(&change->after, &storage->value);
                    change->warm = warmBefore;
                    change->prev = changes->storageChanges;
                    changes->storageChanges = change;
                }
                break;
            case CALLVALUE:
                UPPER(UPPER_P(callContext->top - 1)) = 0;
                LOWER(UPPER_P(callContext->top - 1)) = 0;
                UPPER(LOWER_P(callContext->top - 1)) = callContext->callValue[0];
                LOWER(LOWER_P(callContext->top - 1)) = callContext->callValue[2] | ((uint64_t) callContext->callValue[1] << 32);

                break;
            case SELFBALANCE:
                UPPER(UPPER_P(callContext->top - 1)) = 0;
                LOWER(UPPER_P(callContext->top - 1)) = 0;
                UPPER(LOWER_P(callContext->top - 1)) = callContext->account->balance[0];
                LOWER(LOWER_P(callContext->top - 1)) = callContext->account->balance[2] | ((uint64_t) callContext->account->balance[1] << 32);
                break;
            case BALANCE:
                {
                    account_t *account = warmAccount(callContext, AddressFromUint256(callContext->top - 1));
                    if (account == NULL) {
                        OUT_OF_GAS;
                    }
                    UPPER(UPPER_P(callContext->top - 1)) = 0;
                    LOWER(UPPER_P(callContext->top - 1)) = 0;
                    UPPER(LOWER_P(callContext->top - 1)) = account->balance[0];
                    LOWER(LOWER_P(callContext->top - 1)) = account->balance[2] | ((uint64_t) account->balance[1] << 32);
                }
                break;
            case CALL:
                {
                    data_t input;
                    input.size = LOWER(LOWER_P(callContext->top + 1));
                    uint64_t src = LOWER(LOWER_P(callContext->top + 2));
                    uint64_t dst = LOWER(LOWER_P(callContext->top));
                    uint64_t gas = LOWER(LOWER_P(callContext->top + 5));
                    val_t value;
                    value[0] = UPPER(LOWER_P(callContext->top + 3));
                    value[1] = LOWER(LOWER_P(callContext->top + 3)) >> 32;
                    value[2] = LOWER(LOWER_P(callContext->top + 3));
                    uint64_t outSize = LOWER(LOWER_P(callContext->top - 1));
                    if (!ensureMemory(callContext, src + input.size)) {
                        OUT_OF_GAS;
                    }
                    if (!ensureMemory(callContext, dst + outSize)) {
                        OUT_OF_GAS;
                    }
                    input.content = callContext->memory.uint8s + src;
                    // C_EXTRA
                    address_t to = AddressFromUint256(callContext->top + 4);
                    account_t *toAccount = warmAccount(callContext, to);
                    if (toAccount == NULL) {
                        OUT_OF_GAS;
                    }
                    uint64_t gasCost = 0;
                    if (value[0] || value[1] || value[2]) {
                        gasCost += G_CALLVALUE;
                    }
                    if (AccountDead(toAccount)) {
                        gasCost += G_NEWACCOUNT;
                    }
                    if (gasCost > callContext->gas) {
                        OUT_OF_GAS;
                    }
                    callContext->gas -= gasCost;
                    if (UPPER(UPPER_P(callContext->top + 5)) || LOWER(UPPER_P(callContext->top + 5)) || UPPER(LOWER_P(callContext->top + 5)) || gas > L(callContext->gas)) {
                        gas = L(callContext->gas);
                    }
                    callContext->gas -= gas;
                    if (value[0] || value[1] || value[2]) {
                        gas += G_CALLSTIPEND;
                    }
                    result_t callResult = evmCall(callContext->account->address, gas, to, value, input);
                    callContext->gas += callResult.gasRemaining;
                    callContext->returnData = callResult.returnData;
                    if (callContext->returnData.size < outSize) {
                        outSize = callContext->returnData.size;
                    }
                    memcpy(callContext->memory.uint8s + dst, callResult.returnData.content, outSize);
                    copy256(callContext->top - 1, &callResult.status);
                }
                break;
            case STATICCALL:
                {
                    data_t input;
                    input.size = LOWER(LOWER_P(callContext->top + 1));
                    uint64_t src = LOWER(LOWER_P(callContext->top + 2));
                    uint64_t dst = LOWER(LOWER_P(callContext->top));
                    uint64_t gas = LOWER(LOWER_P(callContext->top + 4));
                    uint64_t outSize = LOWER(LOWER_P(callContext->top - 1));
                    if (!ensureMemory(callContext, src + input.size)) {
                        OUT_OF_GAS;
                    }
                    if (!ensureMemory(callContext, dst + outSize)) {
                        OUT_OF_GAS;
                    }
                    input.content = callContext->memory.uint8s + src;
                    // C_EXTRA
                    address_t to = AddressFromUint256(callContext->top + 3);
                    account_t *toAccount = warmAccount(callContext, to);
                    if (toAccount == NULL) {
                        OUT_OF_GAS;
                    }
                    uint64_t gasCost = 0;
                    if (AccountDead(toAccount)) {
                        gasCost += G_NEWACCOUNT;
                    }
                    if (gasCost > callContext->gas) {
                        OUT_OF_GAS;
                    }
                    callContext->gas -= gasCost;
                    if (UPPER(UPPER_P(callContext->top + 4)) || LOWER(UPPER_P(callContext->top + 4)) || UPPER(LOWER_P(callContext->top + 4)) || gas > L(callContext->gas)) {
                        gas = L(callContext->gas);
                    }
                    callContext->gas -= gas;
                    result_t callResult = evmStaticCall(callContext->account->address, gas, to, input);
                    callContext->gas += callResult.gasRemaining;
                    callContext->returnData = callResult.returnData;
                    if (callContext->returnData.size < outSize) {
                        outSize = callContext->returnData.size;
                    }
                    memcpy(callContext->memory.uint8s + dst, callResult.returnData.content, outSize);
                    copy256(callContext->top - 1, &callResult.status);
                }
                break;
            case RETURN:
                LOWER(LOWER(result.status)) = 1;
                // intentional fallthrough
            case REVERT:
                if (!ensureMemory(callContext, LOWER(LOWER_P(callContext->top + 1)) + LOWER(LOWER_P(callContext->top)))) {
                    OUT_OF_GAS;
                }
                result.returnData.content = callContext->memory.uint8s + LOWER(LOWER_P(callContext->top + 1));
                result.returnData.size = LOWER(LOWER_P(callContext->top));
                return result;
        }
    }
#undef OUT_OF_GAS
}

static void evmRevertStorageChanges(account_t *account, storageChanges_t **changes) {
    if (*changes == NULL) {
        return;
    }
    storage_t *storage = getAccountStorage(account, &(*changes)->key);
    assert(equal256(&storage->value, &(*changes)->after));
    copy256(&storage->value, &(*changes)->before);
    storage->warm = (*changes)->warm;

    evmRevertStorageChanges(account, &(*changes)->prev);
    free(*changes);
    *changes = NULL;
}

static void evmRevert(stateChanges_t **changes) {
    if (*changes == NULL) {
        return;
    }
    account_t *account = getAccount((*changes)->account);
    evmRevertStorageChanges(account, &(*changes)->storageChanges);

    evmRevert(&(*changes)->next);
    free(*changes);
    *changes = NULL;
}

static result_t evmStaticCall(address_t from, uint64_t gas, address_t to, data_t input) {
    context_t *callContext = callstack.next;
    callContext->gas = gas;
    account_t *fromAccount = getAccount(from);

    callContext->top = callContext->bottom;
    callContext->readonly = true;
    callContext->callValue[0] = callContext->callValue[1] = callContext->callValue[2] = 0;
    AddressCopy(callContext->caller, from);
    callContext->account = getAccount(to);
    callContext->code = callContext->account->code;
    callContext->callData = input;
    callContext->returnData.size = 0;

    memory_init(&callContext->memory, 0);

    callstack.next += 1;
    result_t result = doCall(callContext);
    callstack.next -= 1;
    result.gasRemaining = callContext->gas;
    if (zero256(&result.status)) {
        evmRevert(&result.stateChanges);
    }

    return result;
}

result_t evmCall(address_t from, uint64_t gas, address_t to, val_t value, data_t input) {
    context_t *callContext = callstack.next;
    callContext->gas = gas;
    account_t *fromAccount = getAccount(from);
    if (callstack.next == callstack.bottom) {
        callContext->readonly = false;
        callContext->gas = gas - G_TX;
        deductCalldataGas(callContext, &input, false);
        if (gas < callContext->gas) {
            // underflow indicates insufficient initial gas
            fprintf(stderr, "Insufficient intrinsic gas %llu (need %llu)\n", gas, gas - callContext->gas);
            result_t result;
            result.gasRemaining = 0;
            clear256(&result.status);
            result.returnData.size = 0;
            return result;
        }
        fromAccount->warm = evmIteration;
        account_t *toAccount = getAccount(to);
        toAccount->warm = evmIteration;
    } else {
        callContext->readonly = callContext[-1].readonly;
    }

    callContext->top = callContext->bottom;
    BalanceCopy(callContext->callValue, value);
    AddressCopy(callContext->caller, from);
    callContext->account = getAccount(to);
    callContext->code = callContext->account->code;
    callContext->callData = input;
    callContext->returnData.size = 0;

    if (!BalanceSub(fromAccount->balance, value)) {
        fprintf(stderr, "Insufficient intrinsic value [0x%08x%08x%08x] (need [0x%08x%08x%08x])\n",
            fromAccount->balance[0], fromAccount->balance[1], fromAccount->balance[2],
            value[0], value[1], value[2]
        );

        result_t result;
        result.gasRemaining = 0;
        clear256(&result.status);
        result.returnData.size = 0;
        return result;
    }
    BalanceAdd(callContext->account->balance, value);
    memory_init(&callContext->memory, 0);

    callstack.next += 1;
    result_t result = doCall(callContext);
    callstack.next -= 1;
    result.gasRemaining = callContext->gas;
    if (zero256(&result.status)) {
        evmRevert(&result.stateChanges);
    }

    return result;
}

result_t evmCreate(address_t from, uint64_t gas, val_t value, data_t input) {
    context_t *callContext = callstack.next;
    callContext->gas = gas;
    account_t *fromAccount = getAccount(from);
    if (callstack.next == callstack.bottom) {
        callContext->gas -= G_TXCREATE + G_TX;
        deductCalldataGas(callContext, &input, true);
        if (gas < callContext->gas) {
            // underflow indicates insufficient initial gas
            fprintf(stderr, "Insufficient intrinsic gas %llu (need %llu)\n", gas, gas - callContext->gas);
            result_t result;
            result.gasRemaining = 0;
            clear256(&result.status);
            result.returnData.size = 0;
            return result;
        }
        fromAccount->warm = evmIteration;
    }
    callContext->readonly = false;

    callContext->top = callContext->bottom;
    BalanceCopy(callContext->callValue, value);
    AddressCopy(callContext->caller, from);
    callContext->account = createNewAccount(fromAccount);
    callContext->account->warm = evmIteration;
    BalanceAdd(callContext->account->balance, value);
    callContext->code = input;
    callContext->callData.size = 0;
    memory_init(&callContext->memory, 0);
    callContext->returnData.size = 0;

    callstack.next += 1;
    result_t result = doCall(callContext);
    callstack.next -= 1;
    result.gasRemaining = callContext->gas;
    if (!zero256(&result.status)) {
        uint64_t codeGas = result.returnData.size * G_PER_CODEBYTE;
        if (codeGas > callContext->gas) {
            fprintf(stderr, "Insufficient gas to insert code\n");
            LOWER(LOWER(result.status)) = 0;
        } else {
            result.gasRemaining -= codeGas;
            AddressToUint256(&result.status, &callContext->account->address);
            callContext->account->code.size = result.returnData.size;
            callContext->account->code.content = malloc(result.returnData.size);
            memcpy(callContext->account->code.content, result.returnData.content, result.returnData.size);
        }
    }
    if (zero256(&result.status)) {
        evmRevert(&result.stateChanges);
    }
    uint64_t gasUsed = gas - result.gasRemaining;
    if (callstack.next == callstack.bottom) {
        // Apply refund
        uint64_t refund = gasUsed / MAX_REFUND_DIVISOR;
        if (refund > refundCounter) {
            refund = refundCounter;
        }
        result.gasRemaining += refund;
        refundCounter = 0;
    }

    return result;
}

result_t txCall(address_t from, uint64_t gas, address_t to, val_t value, data_t input) {
    account_t *fromAccount = getAccount(from);
    fromAccount->warm = evmIteration;
    result_t result = evmCall(from, gas, to, value, input);
    evmIteration++;
    getAccount(from)->nonce++;
    return result;
}

result_t txCreate(address_t from, uint64_t gas, val_t value, data_t input) {
    result_t result = evmCreate(from, gas, value, input);
    evmIteration++;
    return result;
}
