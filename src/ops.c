#include "hex.h"
#include "ops.h"
#include <stdio.h>

const uint8_t argCount[NUM_OPCODES] = {
    #define OP(index,name,in,out,gas) in,
    OPS
    #undef OP
};
const uint8_t retCount[NUM_OPCODES] = {
    #define OP(index,name,in,out,gas) out,
    OPS
    #undef OP
};
const uint64_t gasCost[NUM_OPCODES] = {
    #define OP(index,name,in,out,gas) gas,
    OPS
    #undef OP
};

const char *opString[NUM_OPCODES] = {
        #define OP(index,name,in,out,gas) #name,
        OPS
        #undef OP
};

op_t parseOp(const char *start, const char **endOut) {
    switch (*(uint32_t *)start) {
        case 'RDDA': *endOut = start + 7; return ADDRESS;
        case 'MDDA': *endOut = start + 6; return ADDMOD;
        case 'ALAB': *endOut = start + 7; return BALANCE;
        case 'COLB': *endOut = start + 9; return BLOCKHASH;
        case 'ETYB': *endOut = start + 4; return BYTE;
        case 'LLAC':
            switch (*(uint16_t *)(start += 4)) {
                case 'OC': *endOut = start + 4; return CALLCODE;
                case 'AD':
                    *endOut = start + 8;
                    switch (*(uint32_t *)(start += 4)) {
                        case 'DAOL': return CALLDATALOAD;
                        case 'YPOC': return CALLDATACOPY;
                        case 'EZIS': return CALLDATASIZE;
                    }
                case 'RE': *endOut = start + 2; return CALLER;
                case 'AV': *endOut = start + 5; return CALLVALUE;
            }
            *endOut = start;
            return CALL;
        case 'IAHC':
            *endOut = start + 7;
            return CHAINID;
        case 'EDOC':
            switch (*(uint32_t *)(start += 4)) {
                case 'EZIS': *endOut = start + 4; return CODESIZE;
                case 'YPOC': *endOut = start + 4; return CODECOPY;
            }
        case 'NIOC': *endOut = start + 8; return COINBASE;
        case 'AERC': 
            start += 5;
            if ('2E' == *(uint16_t *)start) {
                *endOut = start + 2;
                return CREATE2;
            } else {
                *endOut = start + 1;
                return CREATE;
            }
        case 'ELED': *endOut = start + 12; return DELEGATECALL;
        case '1PUD': 
            *endOut = start + 5;
            switch (*(uint8_t *)(start += 4)) {
                case '0': return DUP10;
                case '1': return DUP11;
                case '2': return DUP12;
                case '3': return DUP13;
                case '4': return DUP14;
                case '5': return DUP15;
                case '6': return DUP16;
            }
            *endOut = start; return DUP1;
        case '2PUD': *endOut = start + 4; return DUP2;
        case '3PUD': *endOut = start + 4; return DUP3;
        case '4PUD': *endOut = start + 4; return DUP4;
        case '5PUD': *endOut = start + 4; return DUP5;
        case '6PUD': *endOut = start + 4; return DUP6;
        case '7PUD': *endOut = start + 4; return DUP7;
        case '8PUD': *endOut = start + 4; return DUP8;
        case '9PUD': *endOut = start + 4; return DUP9;
        case 'PAWS': 
            *endOut = start + 5;
            switch (*(uint8_t *)(start += 4)) {
                case '2': return SWAP2;
                case '3': return SWAP3;
                case '4': return SWAP4;
                case '5': return SWAP5;
                case '6': return SWAP6;
                case '7': return SWAP7;
                case '8': return SWAP8;
                case '9': return SWAP9;
                case '1': 
                    *endOut = start + 2;
                    switch (*(uint8_t *)(start += 1)) {
                        case '0': return SWAP10;
                        case '1': return SWAP11;
                        case '2': return SWAP12;
                        case '3': return SWAP13;
                        case '4': return SWAP14;
                        case '5': return SWAP15;
                        case '6': return SWAP16;
                    }
                    *endOut = start;
                    return SWAP1;
            }
        case 'CTXE': 
            switch (*(uint32_t *)(start += 4)) {
                case 'CEDO': *endOut = start + 7; return EXTCODECOPY;
                case 'HEDO': *endOut = start + 7; return EXTCODEHASH;
                case 'SEDO': *endOut = start + 7; return EXTCODESIZE;
            }
        case 'ESAB':
            *endOut = start + 7;
            return BASEFEE;
        case 'ESSA':
            *endOut = start + 11;
            return (op_t)hexString16ToUint8(start + 9);
        case 'LSAG': *endOut = start + 8; return GASLIMIT;
        case 'PSAG': *endOut = start + 8; return GASPRICE;
        case 'AVNI': *endOut = start + 7; return INVALID;
        case 'EZSI': *endOut = start + 6; return ISZERO;
        case 'PMUJ':
            switch (*(uint8_t *)(start + 4)) {
                case 'D': *endOut = start + 8; return JUMPDEST;
                case 'I': *endOut = start + 5; return JUMPI;
            }
            *endOut = start + 4;
            return JUMP;
        case '0GOL': *endOut = start + 4; return LOG0;
        case '1GOL': *endOut = start + 4; return LOG1;
        case '2GOL': *endOut = start + 4; return LOG2;
        case '3GOL': *endOut = start + 4; return LOG3;
        case '4GOL': *endOut = start + 4; return LOG4;
        case 'AOLM': *endOut = start + 5; return MLOAD;
        case 'ZISM': *endOut = start + 5; return MSIZE;
        case 'POCM': *endOut = start + 5; return MCOPY;
        case 'AOLT': *endOut = start + 5; return TLOAD;
        case 'OTST': *endOut = start + 6; return TSTORE;
        case 'OTSM':
            switch (*(uint8_t *)(start + 6)) {
                case '8': *endOut = start + 7; return MSTORE8;
            }
            *endOut = start + 6;
            return MSTORE;
        case 'MLUM': *endOut = start + 6; return MULMOD;
        case 'BMUN': *endOut = start + 6; return NUMBER;
        case 'GIRO': *endOut = start + 6; return ORIGIN;
        case 'VERP': *endOut = start + 10; return PREVRANDAO;
        case 'HSUP':
            *endOut = start + 5;
            switch (*(uint8_t *)(start + 4)) {
                case '2':
                    *endOut = start + 6;
                    switch (*(uint8_t *)(start + 5)) {
                        case '0': return PUSH20;
                        case '1': return PUSH21;
                        case '2': return PUSH22;
                        case '3': return PUSH23;
                        case '4': return PUSH24;
                        case '5': return PUSH25;
                        case '6': return PUSH26;
                        case '7': return PUSH27;
                        case '8': return PUSH28;
                        case '9': return PUSH29;
                    }
                    *endOut = start + 5;
                    return PUSH2;
                case '3':
                    *endOut = start + 6;
                    switch (*(uint8_t *)(start + 5)) {
                        case '0': return PUSH30;
                        case '1': return PUSH31;
                        case '2': return PUSH32;
                    }
                    *endOut = start + 5;
                    return PUSH3;
                case '0': return PUSH0;
                case '4': return PUSH4;
                case '5': return PUSH5;
                case '6': return PUSH6;
                case '7': return PUSH7;
                case '8': return PUSH8;
                case '9': return PUSH9;
                case '1':
                    *endOut = start + 6;
                    switch (*(uint8_t *)(start + 5)) {
                        case '0': return PUSH10;
                        case '1': return PUSH11;
                        case '2': return PUSH12;
                        case '3': return PUSH13;
                        case '4': return PUSH14;
                        case '5': return PUSH15;
                        case '6': return PUSH16;
                        case '7': return PUSH17;
                        case '8': return PUSH18;
                        case '9': return PUSH19;
                    }
                    *endOut = start + 5;
                    return PUSH1;
            }
        case 'UTER':
            *endOut = start + 14;
            switch (*(uint32_t *)(start + 8)) {
                case 'ISAT': return RETURNDATASIZE;
                case 'OCAT': return RETURNDATACOPY;
            }
            *endOut = start + 6;
            return RETURN;
        case 'EVER': *endOut = start + 6; return REVERT;
        case 'FLES':
            switch (*(uint32_t *)(start + 4)) {
                case 'ALAB': *endOut = start + 11; return SELFBALANCE;
                case 'TSED': *endOut = start + 12; return SELFDESTRUCT;
            }
        case 'NGIS': *endOut = start + 10; return SIGNEXTEND;
        case 'VIDS': *endOut = start + 4; return SDIV;
        case '3AHS': *endOut = start + 4; return SHA3;
        case 'AOLS': *endOut = start + 5; return SLOAD;
        case 'OTSS': *endOut = start + 6; return SSTORE;
        case 'TATS': *endOut = start + 10; return STATICCALL;
        case 'DOMS': *endOut = start + 4; return SMOD;
        case 'POTS': *endOut = start + 4; return STOP;
        case 'EMIT': *endOut = start + 9; return TIMESTAMP;
    }
    *endOut = start + 3;
    switch ((*(uint32_t *)start) & 0x00ffffff) {
        case 'DDA': return ADD;
        case 'DNA': return AND;
        case 'VID': return DIV;
        case 'PXE': return EXP;
        case 'BUS': return SUB;
        case 'ROX': return XOR;
        case 'SAG': return GAS;
        case 'DOM': return MOD;
        case 'LHS': return SHL;
        case 'RAS': return SAR;
        case 'RHS': return SHR;
        case 'LUM': return MUL;
        case 'TON': return NOT;
        case 'POP': return POP;
        case 'TGS': return SGT;
        case 'TLS': return SLT;
    }
    *endOut = start + 2;
    switch (*(uint16_t *)start) {
        case 'QE': return EQ;
        case 'TG': return GT;
        case 'TL': return LT;
        case 'RO': return OR;
        case 'CP': return PC;
    }
    fputs("Unknown op: ", stderr);
    fputs(start, stderr);
    fputc('\n', stderr);
    return STOP;
}
