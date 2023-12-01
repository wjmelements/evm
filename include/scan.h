#include "ops.h"
#include <stdio.h>

void scanInit();
op_t scanNextOp(const char **iter);
int scanValid(const char **iter);
void scanFinalize(op_t *begin, uint32_t *programLength);

void fprintLabels(FILE *);
