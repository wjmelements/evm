#include "precompiles.h"

const char *precompileName[KNOWN_PRECOMPILES] = {
    #define PRECOMPILE(name,address,supported) #name,
    PRECOMPILES
    #undef PRECOMPILE
};

int PrecompileIsSupported(precompile_t precompile) {
    switch (precompile) {
#define PRECOMPILE(name,address,supported) case name: return supported;
        PRECOMPILES
#undef PRECOMPILE
        default:
            return 0;
    }
}
