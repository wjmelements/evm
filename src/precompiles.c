#include "precompiles.h"

const char *precompileName[NUM_PRECOMPILES] = {
    #define PRECOMPILE(name,address) #name,
    PRECOMPILES
    #undef PRECOMPILE
};
