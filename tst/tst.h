#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define assertStderr(expectedErr, statement)\
    int rw[2];\
    pipe(rw);\
    int savedStderr = dup(2);\
    close(2);\
    dup2(rw[1], 2);\
    close(rw[1]);\
    statement;\
    close(2);\
    dup2(savedStderr, 2);\
    clearerr(stderr);\
    close(savedStderr);\
    size_t strSize = strlen(expectedErr) + 1;\
    char *actualErr = malloc(strSize);\
    ssize_t red = read(rw[0], actualErr, strSize);\
    if (red == -1) {\
        perror("read");\
        exit(1);\
    }\
    actualErr[red] = 0;\
    if (red != strSize - 1) {\
        fprintf(stderr, "stderr length mismatch\nexpected[%zu]: \"%s\"\nactual[%zd]: \"%s\"\n", strSize, expectedErr, red, actualErr);\
        exit(1);\
    }\
    close(rw[0]);\
    if (memcmp(expectedErr, actualErr, strSize) != 0) {\
        fprintf(stderr, "stderr mismatch\nexpected[%zu]: \"%s\"\nactual[%zd]: \"%s\"\n", strSize, expectedErr, red, actualErr);\
        exit(1);\
    }
