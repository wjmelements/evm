#include "hex.h"

#include <assert.h>

int main() {
    for (char a = 'a'; a <= 'f'; a++) {
        assert(isHex(a));
    }
    for (char a = 'A'; a <= 'F'; a++) {
        assert(isHex(a));
    }
    for (char a = '0'; a <= '9'; a++) {
        assert(isHex(a));
    }
    return 0;
}
