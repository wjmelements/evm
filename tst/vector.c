#include <assert.h>
#include "vector.h"

typedef int sint_t;
VECTOR(sint, ints);
typedef char schar_t;
VECTOR(schar, string);

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
    for (int i = 100; i--> 0;) {
        assert(ints_pop(&naturals) == i);
    }
    string_t alpha;
    string_init(&alpha, 4);
    for (char i = 'a'; i < 'z'; i++) {
        for (char j = 'a'; j < i; j++) {
            assert(alpha.schars[j - 'a'] == j);
        }
        string_append(&alpha, i);
        for (char j = 'a'; j <= i; j++) {
            assert(alpha.schars[j - 'a'] == j);
        }
    }

    return 0;
}
