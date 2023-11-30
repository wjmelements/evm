#define G_ZERO 0
#define G_JUMPDEST 1
#define G_BASE 2
#define G_VERYLOW 3
#define G_LOW 5
#define G_MID 8
#define G_HIGH 10
#define G_ACCESS 100
#define G_AUTH 3100
#define G_ACCESSLIST_ACCOUNT 2400
#define G_ACCESSLIST_STORAGE 1900
#define G_COLD_ACCOUNT 2600
#define G_COLD_STORAGE 2100
#define G_SSET 20000
#define G_SRESET 2900
#define R_CLEAR (G_SRESET + G_ACCESSLIST_STORAGE)
#define R_SELFDESTRUCT 24000
#define G_SELFDESTRUCT 5000
#define G_CREATE 32000
#define G_PER_CODEBYTE 200
#define G_CALLVALUE 9000
#define G_CALLSTIPEND 2300
#define G_NEWACCOUNT 25000
#define G_EXP 10
#define G_EXPBYTE 50
#define G_MEM 3
#define G_QUADDIV 512
#define G_TX 21000
#define G_TXCREATE 32000
#define G_INITCODEWORD 2
#define G_CALLDATAZERO 4
#define G_CALLDATA 16
#define G_CALLDATANONZERO 12
#define G_LOG 375
#define G_LOGDATA 8
#define G_LOGTOPIC 375
#define G_KECCAK 30
#define G_KECCAK_WORD 6
#define G_COPY 3
#define G_BLOCKHASH 20
#define MAX_REFUND_DIVISOR 5

static inline uint64_t L(uint64_t gas) {
    return gas - gas / 64;
}
