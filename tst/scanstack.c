#include "scanstack.h"
#include <assert.h>

int main() {
    scanstackPush(DUP3);
    assert(DUP3 == scanstackPop());
    return 0;
}
