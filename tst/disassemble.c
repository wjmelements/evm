#include "disassemble.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


static void checkDisassembly(const char *bytecode, const char *expected) {
    fflush(stdout);
    int savedStdout = dup(STDOUT_FILENO);
    FILE *out = tmpfile();
    dup2(fileno(out), STDOUT_FILENO);

    disassembleInit();
    const char *iter = bytecode;
    while (disassembleValid(&iter)) {
        disassembleNextOp(&iter);
    }
    disassembleFinalize();

    fflush(stdout);
    dup2(savedStdout, STDOUT_FILENO);
    close(savedStdout);

    char actual[256];
    rewind(out);
    size_t length = fread(actual, 1, sizeof(actual) - 1, out);
    actual[length] = '\0';
    fclose(out);

    if (strcmp(actual, expected)) {
        fprintf(stderr, "%s: expected \"%s\" got \"%s\"\n", bytecode, expected, actual);
    }
    assert(strcmp(actual, expected) == 0);
}

int main() {
    // decimal when there is no leading zero
    checkDisassembly("6001", "1\n");
    checkDisassembly("60ff", "255\n");
    checkDisassembly("610100", "256\n");
    checkDisassembly("61ff00", "65280\n");
    checkDisassembly("62010000", "65536\n");
    checkDisassembly("62ffffff", "16777215\n");

    // hex when the leading byte is zero
    checkDisassembly("6000", "0x00\n");
    checkDisassembly("6100", "0x0000\n");
    checkDisassembly("610000", "0x0000\n");
    checkDisassembly("610001", "0x0001\n");
    checkDisassembly("6100ff", "0x00ff\n");
    checkDisassembly("62000001", "0x000001\n");
    checkDisassembly("62000100", "0x000100\n");

    // hex for pushes wider than 3 bytes
    checkDisassembly("6301020304", "0x01020304\n");
    checkDisassembly("6300000001", "0x00000001\n");
    checkDisassembly(
        "7f0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20",
        "0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20\n"
    );

    // truncated constants are zero-padded
    checkDisassembly("60", "0x00\n");
    checkDisassembly("6201", "65536\n");
    checkDisassembly("6301", "0x01000000\n");

    // whitespace and 0x prefixes within the constant
    checkDisassembly("61 00 01", "0x0001\n");
    checkDisassembly("0x61 0x00 0x01", "0x0001\n");
    checkDisassembly("61 ff 00", "65280\n");
    checkDisassembly("0x61\n0xff\n0x00", "65280\n");

    // consecutive pushes
    checkDisassembly("60016002", "1\n2\n");
    checkDisassembly("6000600161000262000003", "0x00\n1\n0x0002\n0x000003\n");
    return 0;
}
