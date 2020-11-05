#include <assert.h>
#include "vector.h"

typedef int sint_t;
VECTOR(sint, ints);

int main() {
    ints_t naturals;
    ints_init(&naturals, 1);
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < i; j++) {
            assert(naturals.sints[j] == j);
        }
        ints_append(&naturals, i);
        for (int j = 0; j <= i; j++) {
            assert(naturals.sints[j] == j);
        }
    }
    return 0;
}
