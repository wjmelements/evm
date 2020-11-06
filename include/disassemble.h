#include "ops.h"
#include "vector.h"
typedef char schar_t;
VECTOR(schar, statement);
typedef statement_t statement_t;
VECTOR(statement, statement_stack);


void disassembleInit();
void disassembleNextOp(const char **iter);
int disassembleValid(const char **iter);
void disassembleFinalize();
