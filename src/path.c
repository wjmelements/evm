#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "data.h"
#include "hex.h"

static const char *selfPath;
static const char *derivedPath = NULL;

void pathInit(const char *_selfPath) {
    selfPath = _selfPath;
}

const char *derivePath() {
    if (derivedPath != NULL) {
        return derivedPath;
    }
    if (!selfPath) {
        fprintf(stderr, "must pathInit\n");
        exit(1);
    }
    if (selfPath[0] == '/') {
        // absolute path
        derivedPath = selfPath;
        return derivedPath;
    }
    const char *pathWalk = selfPath;
    char *relativePath = malloc(MAXPATHLEN + 1);
    while (*pathWalk) {
        if (*pathWalk == '/') {
            // relative path
            if (getcwd(relativePath, MAXPATHLEN + 1) == NULL) {
                perror("getcwd");
                exit(1);
            }
            size_t offset = strlen(relativePath);
            relativePath[offset++] = '/';
            relativePath[offset] = '\0';
            strcpy(relativePath + offset, selfPath);
            derivedPath = relativePath;
            return derivedPath;
        }
        pathWalk++;
    }
    // look through $PATH
    char *path = getenv("PATH");
    struct stat statResult;
    while (*path) {
        char *next = strchr(path,':');
        if (next == NULL) {
            break;
        }
        char *end = stpncpy(relativePath, path, next - path);
        *end++ = '/';
        strcpy(end, selfPath);
        if (stat(relativePath, &statResult) == -1) {
            //perror(relativePath);
            path = next + 1;
        } else {
            derivedPath = relativePath;
            return derivedPath;
        }
    }
    fputs("evm: could not find evm\n", stderr);
    _exit(-1);
}

// evm -c $constructPath
data_t defaultConstructorForPath(const char *path) {
    int rw[2];
    if (pipe(rw) == -1) {
        perror("pipe");
        _exit(-1);
    }
    derivePath();
    pid_t pid = fork();
    if (pid == 0) {
        close(1);
        close(rw[0]);
        if (dup2(rw[1], 1) == -1) {
            perror("dup2");
            _exit(-1);
        }
        // child
        char *const args[4] = {
            (char *) derivedPath,
            "-c",
            (char *) path,
            NULL
        };
        if (execve(derivedPath, args, NULL) == -1) {
            perror(derivedPath);
            _exit(-1);
        }
    }
    close(rw[1]);
    int status;
    pid_t finished = wait(&status);
    if (finished == -1) {
        perror("wait");
        _exit(-1);
    }
    size_t bufferSize = 0x10000;
    data_t input;
    input.size = 0;
    char *content = malloc(bufferSize);
    while (1) {
        ssize_t red = read(rw[0], content + input.size, bufferSize - input.size);
        if (red == -1) {
            perror("read");
            _exit(-1);
        }
        if (red == 0) {
            break;
        }
        input.size += red;
        if (input.size == bufferSize) {
            char *next = malloc(bufferSize * 2);
            memcpy(next, content, input.size);
            free(content);
            content = next;
            bufferSize *= 2;
        }
    }
    for (size_t i = 0; i < input.size / 2; i++) {
        content[i] = hexString16ToUint8(content + i * 2);
    }
    input.content = (uint8_t *)content;
    input.size /= 2;

    return input;
}
