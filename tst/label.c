#include "labelQueue.h"


#include <assert.h>


int main() {
    assert(labelCount == 0);
    assert(firstLabelAfter(0) == 0);

    const char *label = "hello";
    jump_t jump1;
    jump1.label.start = label;
    jump1.label.length = 5;
    jump1.programCounter = 0;
    registerLabel(jump1);
    assert(labelCount == 1);
    assert(labels[0].length == jump1.label.length);
    assert(labelLocations[0] == 0);
    assert(firstLabelAfter(0) == 0);


    incrementLabelLocations(0, 1);
    assert(labelLocations[0] == 1);
    assert(firstLabelAfter(0) == 0);
    //assert(firstLabelAfter(2) == 1);

    const char *label2 = "world";
    jump_t jump2;
    jump2.label.start = label2;
    jump2.label.length = 5;
    jump2.programCounter = 254;
    registerLabel(jump2);
    assert(labelCount == 2);
    assert(labels[0].length == jump2.label.length);
    assert(labelLocations[0] == 1);
    assert(labelLocations[1] == 254);
    incrementLabelLocations(10, 3);
    assert(labelLocations[0] == 1);
    assert(labelLocations[1] == 257);
    assert(firstLabelAfter(0) == 0);
    assert(firstLabelAfter(256) == 1);
    //assert(firstLabelAfter(258) == 2);
}
