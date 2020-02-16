#include "scanstack.h"
#include <assert.h>

int main() {
    assert(scanstackEmpty());
    scanstackPush(DUP3);
    assert(!scanstackEmpty());
    assert(DUP3 == scanstackPop());
    assert(scanstackEmpty());
    return 0;
}
