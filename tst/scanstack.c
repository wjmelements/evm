#include "data.h"
#include "scanstack.h"
#include <assert.h>

int main() {
    assert(scanstackEmpty());
    scanstackPush(DUP3);
    assert(!scanstackEmpty());
    assert(DUP3 == scanstackPop());
    assert(scanstackEmpty());

    data_t data;
    data.size = 2;
    data.content = malloc(2);
    data.content[0] = STOP;
    data.content[1] = PUSH0;
    scanstackPushData(&data);
    assert(!scanstackEmpty());
    assert(STOP == scanstackPop());
    assert(!scanstackEmpty());
    assert(PUSH0 == scanstackPop());
    assert(scanstackEmpty());
    return 0;
}
