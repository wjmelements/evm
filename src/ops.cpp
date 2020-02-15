#include "hex.h"
#include "ops.h"
#include <stdio.h>

const uint8_t argCount[NUM_OPCODES] = {
    #define OP(name,in,out) in,
    OPS
    #undef OP
};
const uint8_t retCount[NUM_OPCODES] = {
    #define OP(name,in,out) out,
    OPS
    #undef OP
};



#define SORTED_OPS \
OP(ADD,2,1) \
OP(ADDRESS,0,1) \
OP(AND,2,1) \
OP(ADDMOD,3,1) \
OP(BALANCE,1,1) \
OP(BLOCKHASH,1,1) \
OP(BYTE,2,1) \
OP(CALL,7,1) \
OP(CALLCODE,7,1) \
OP(CALLDATALOAD,1,1) \
OP(CALLDATACOPY,3,0) \
OP(CALLDATASIZE,0,1) \
OP(CALLER,0,1) \
OP(CALLVALUE,0,1) \
OP(CODESIZE,0,1) \
OP(CODECOPY,3,0) \
OP(COINBASE,0,1) \
OP(CREATE,3,1) \
OP(DELEGATECALL,6,1) \
OP(DIFFICULTY,0,1) \
OP(DIV,2,1) \
OP(DUP1,0,1) \
OP(DUP2,0,1) \
OP(DUP3,0,1) \
OP(DUP4,0,1) \
OP(DUP5,0,1) \
OP(DUP6,0,1) \
OP(DUP7,0,1) \
OP(DUP8,0,1) \
OP(DUP9,0,1) \
OP(DUP10,0,1) \
OP(DUP11,0,1) \
OP(DUP12,0,1) \
OP(DUP13,0,1) \
OP(DUP14,0,1) \
OP(DUP15,0,1) \
OP(DUP16,0,1) \
OP(EQ,2,1) \
OP(EXP,2,1) \
OP(EXTCODESIZE,1,1) \
OP(EXTCODECOPY,4,0) \
OP(GAS,0,1) \
OP(GASLIMIT,0,1) \
OP(GASPRICE,0,1) \
OP(GT,2,1) \
OP(INVALID,0,0) \
OP(ISZERO,2,1) \
OP(JUMP,1,0) \
OP(JUMPDEST,0,0) \
OP(JUMPI,2,0) \
OP(LOG0,2,0) \
OP(LOG1,3,0) \
OP(LOG2,4,0) \
OP(LOG3,5,0) \
OP(LOG4,6,0) \
OP(LT,2,1) \
OP(MLOAD,1,1) \
OP(MSIZE,0,1) \
OP(MOD,2,1) \
OP(MSTORE,2,0) \
OP(MSTORE8,2,0) \
OP(MUL,2,1) \
OP(MULMOD,3,1) \
OP(NOT,2,1) \
OP(NUMBER,0,1) \
OP(OR,2,1) \
OP(ORIGIN,0,1) \
OP(PC,0,1) \
OP(PUSH1,0,1) \
OP(PUSH2,0,1) \
OP(PUSH3,0,1) \
OP(PUSH4,0,1) \
OP(PUSH5,0,1) \
OP(PUSH6,0,1) \
OP(PUSH7,0,1) \
OP(PUSH8,0,1) \
OP(PUSH9,0,1) \
OP(PUSH10,0,1) \
OP(PUSH11,0,1) \
OP(PUSH12,0,1) \
OP(PUSH13,0,1) \
OP(PUSH14,0,1) \
OP(PUSH15,0,1) \
OP(PUSH16,0,1) \
OP(PUSH17,0,1) \
OP(PUSH18,0,1) \
OP(PUSH19,0,1) \
OP(PUSH20,0,1) \
OP(PUSH21,0,1) \
OP(PUSH22,0,1) \
OP(PUSH23,0,1) \
OP(PUSH24,0,1) \
OP(PUSH25,0,1) \
OP(PUSH26,0,1) \
OP(PUSH27,0,1) \
OP(PUSH28,0,1) \
OP(PUSH29,0,1) \
OP(PUSH30,0,1) \
OP(PUSH31,0,1) \
OP(PUSH32,0,1) \
OP(POP,1,0) \
OP(RETURN,2,0) \
OP(RETURNDATACOPY,3,0) \
OP(RETURNDATASIZE,0,1) \
OP(REVERT,2,0) \
OP(SELFDESTRUCT,1,0) \
OP(SIGNEXTEND,2,1) \
OP(SDIV,2,1) \
OP(SGT,2,1) \
OP(SHA3,2,1) \
OP(SLOAD,1,1) \
OP(SLT,2,1) \
OP(SSTORE,2,0) \
OP(STATICCALL,6,1) \
OP(SMOD,2,1) \
OP(STOP,0,0) \
OP(SUB,2,1) \
OP(SWAP1,0,0) \
OP(SWAP2,0,0) \
OP(SWAP3,0,0) \
OP(SWAP4,0,0) \
OP(SWAP5,0,0) \
OP(SWAP6,0,0) \
OP(SWAP7,0,0) \
OP(SWAP8,0,0) \
OP(SWAP9,0,0) \
OP(SWAP10,0,0) \
OP(SWAP11,0,0) \
OP(SWAP12,0,0) \
OP(SWAP13,0,0) \
OP(SWAP14,0,0) \
OP(SWAP15,0,0) \
OP(SWAP16,0,0) \
OP(TIMESTAMP,0,1) \
OP(XOR,2,1)


#include <stdio.h>
op_t opFromString(const char *str) {
    printf("%s: %u\n", str, *(uint32_t *)str);
    
    switch (*(uint32_t *)str) {
        case 'DDA': return ADD;
        case 'RDDA': return ADDRESS;
        case 'DNA': return AND;
        case 'MDDA': return ADDMOD;
        case 'ALAB': return BALANCE;
        case 'COLB': return BLOCKHASH;
        case 'ETYB': return BYTE;
        case 'LLAC':
            if (*(str + 4) == '\0') {
                return CALL;
            }
            switch (*(uint16_t *)(str += 4)) {
                case 'OC': return CALLCODE;
                case 'AD':
                    switch (*(uint32_t *)(str += 4)) {
                        case 'DAOL': return CALLDATALOAD;
                        case 'YPOC': return CALLDATACOPY;
                        case 'EZIS': return CALLDATASIZE;
                    }
                case 'RE': return CALLER;
                case 'AV': return CALLVALUE;
            }
        case 'EDOC':
            switch (*(uint32_t *)(str += 4)) {
                case 'EZIS': return CODESIZE;
                case 'YPOC': return CODECOPY;
            }
        case 'NIOC': return COINBASE;
        case 'AERC': return CREATE;
        case 'ELED': return DELEGATECALL;
        case 'FFID': return DIFFICULTY;
        case 'VID': return DIV;
        case '1PUD': 
            switch (*(uint8_t *)(str += 4)) {
                case 0: return DUP1;
                case '0': return DUP10;
                case '1': return DUP11;
                case '2': return DUP12;
                case '3': return DUP13;
                case '4': return DUP14;
                case '5': return DUP15;
                case '6': return DUP16;
            }
        case '2PUD': return DUP2;
        case '3PUD': return DUP3;
        case '4PUD': return DUP4;
        case '5PUD': return DUP5;
        case '6PUD': return DUP6;
        case '7PUD': return DUP7;
        case '8PUD': return DUP8;
        case '9PUD': return DUP9;
        case 'PAWS': 
            switch (*(uint16_t *)(str += 4)) {
                case '1': return SWAP1;
                case '2': return SWAP2;
                case '3': return SWAP3;
                case '4': return SWAP4;
                case '5': return SWAP5;
                case '6': return SWAP6;
                case '7': return SWAP7;
                case '8': return SWAP8;
                case '9': return SWAP9;
                case '01': return SWAP10;
                case '11': return SWAP11;
                case '21': return SWAP12;
                case '31': return SWAP13;
                case '41': return SWAP14;
                case '51': return SWAP15;
                case '61': return SWAP16;
            }
        case 'QE': return EQ;
        case 'PXE': return EXP;
        case 'CTXE': 
            switch (*(uint32_t *)(str += 4)) {
                case 'CEDO': return EXTCODECOPY;
                case 'SEDO': return EXTCODESIZE;
            }
        case 'SAG': return GAS;
        case 'ESSA':
            return (op_t)hexString16ToUint8(str + 9);
        case 'LSAG': return GASLIMIT;
        case 'PSAG': return GASPRICE;
        case 'AVNI': return INVALID;
        case 'EZSI': return ISZERO;
        case 'PMUJ':
            switch (*(uint8_t *)(str + 4)) {
                case '\0': return JUMP;
                case 'D': return JUMPDEST;
                case 'I': return JUMPI;
            }
        case '0GOL': return LOG0;
        case '1GOL': return LOG1;
        case '2GOL': return LOG2;
        case '3GOL': return LOG3;
        case '4GOL': return LOG4;
        case 'AOLM': return MLOAD;
        case 'ZISM': return MSIZE;
        case 'DOM': return MOD;
        case 'OTSM':
            switch (*(uint8_t *)(str + 6)) {
                case '\0': return MSTORE;
                case '8': return MSTORE8;
            }
        case 'LUM': return MUL;
        case 'MLUM': return MULMOD;
        case 'TON': return NOT;
        case 'BMUN': return NUMBER;
        case 'GIRO': return ORIGIN;
        case 'HSUP':
            switch (*(uint16_t *)(str + 4)) {
                case '1': return PUSH1;
                case '2': return PUSH2;
                case '3': return PUSH3;
                case '4': return PUSH4;
                case '5': return PUSH5;
                case '6': return PUSH6;
                case '7': return PUSH7;
                case '8': return PUSH8;
                case '9': return PUSH9;
                case '01': return PUSH10;
                case '11': return PUSH11;
                case '21': return PUSH12;
                case '31': return PUSH13;
                case '41': return PUSH14;
                case '51': return PUSH15;
                case '61': return PUSH16;
                case '71': return PUSH17;
                case '81': return PUSH18;
                case '91': return PUSH19;
                case '02': return PUSH20;
                case '12': return PUSH21;
                case '22': return PUSH22;
                case '32': return PUSH23;
                case '42': return PUSH24;
                case '52': return PUSH25;
                case '62': return PUSH26;
                case '72': return PUSH27;
                case '82': return PUSH28;
                case '92': return PUSH29;
                case '03': return PUSH30;
                case '13': return PUSH31;
                case '23': return PUSH32;
            }
        case 'POP': return POP;
        case 'UTER':
            if ('\0' == *(uint8_t *)(str + 6)) return RETURN;
            switch (*(uint32_t *)(str + 8)) {
                case 'ISAT': return RETURNDATASIZE;
                case 'OCAT': return RETURNDATACOPY;
            }
        case 'EVER': return REVERT;
        case 'FLES': return SELFDESTRUCT;
        case 'NGIS': return SIGNEXTEND;
        case 'VIDS': return SDIV;
        case 'TGS': return SGT;
        case '3AHS': return SHA3;
        case 'AOLS': return SLOAD;
        case 'TLS': return SLT;
        case 'OTSS': return SSTORE;
        case 'TATS': return STATICCALL;
        case 'DOMS': return SMOD;
        case 'POTS': return STOP;
        case 'BUS': return SUB;
        case 'EMIT': return TIMESTAMP;
        case 'ROX': return XOR;
    }
    switch (*(uint16_t *)str) {
        case 'QE': return EQ;
        case 'TG': return GT;
        case 'TL': return LT;
        case 'RO': return OR;
        case 'CP': return PC;
    }
    fputs("Unknown op: ", stderr);
    fputs(str, stderr);
    fputc('\n', stderr);
    return NUM_OPCODES;
}
/*
*/
