#include "precompiles.h"
#include <stdio.h>

const char *precompiles = ""
#define PRECOMPILE(name,address,supported) "| `" #name "` | `" #address "` | " #supported " |\n"
PRECOMPILES
#undef PRECOMPILE
;


int main() {
    fputs(precompiles, stdout);
}
