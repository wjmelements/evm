#define PRECOMPILES \
PRECOMPILE(HOLE,0x0) \
PRECOMPILE(ECRECOVER,0x1) \
PRECOMPILE(SHA2_256,0x2) \
PRECOMPILE(RIPEMD160,0x3) \
PRECOMPILE(IDENTITY,0x4) \
PRECOMPILE(MODEXP,0x5) \
PRECOMPILE(EC_ADD,0x6) \
PRECOMPILE(EC_MUL,0x7) \
PRECOMPILE(EC_PAIRING,0x8) \
PRECOMPILE(BLACK2F,0x9) \
PRECOMPILE(ZKG_POINT,0xa)

typedef enum precompile {
    #define PRECOMPILE(name,address) name,
    PRECOMPILES
    #undef PRECOMPILE
    NUM_PRECOMPILES
} precompile_t;

extern const char *precompileName[NUM_PRECOMPILES];
