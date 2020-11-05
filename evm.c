#include "scan.h"
#include "disassemble.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define CONSTRUCTOR_OFFSET 0x1000
#define PROGRAM_BUFFER_LENGTH 0x8000
op_t ops[PROGRAM_BUFFER_LENGTH];


int wrapConstructor = 0;
int inverse = 0;
void assemble(const char *contents) {
    op_t *programStart = &ops[CONSTRUCTOR_OFFSET];
    uint32_t programLength = 0;
    for (; scanValid(&contents); programLength++) {
        if (programLength > (PROGRAM_BUFFER_LENGTH - CONSTRUCTOR_OFFSET)) {
            fprintf(stderr, "Program size exceeds limit; terminating");
            break;
        }
        programStart[programLength] = scanNextOp(&contents);
    }
    scanFinalize(programStart, &programLength);
    if (wrapConstructor) {
        if (programLength < 0x20) {
            // PUSHx<>3d5260xx60xxf3
            programStart -= 1;
            *programStart = (PUSH1 - 1) + programLength;
            *((uint32_t *)(programStart + programLength + 1)) = 0x0060523d + (programLength << 24);
            *((uint32_t *)(programStart + programLength + 5)) = 0xf30060 + ((32 - programLength) << 8);
            programLength += 8;
        } else if (programLength == 0x20) {
            // 7f<>3d5260203df3
            programStart -= 1;
            *programStart = PUSH32;
            *((uint32_t *)(programStart + programLength + 1)) = 0x2060523d;
            *((uint16_t *)(programStart + programLength + 5)) = 0xf33d;
            programLength += 7;
        } else {
            programStart -= 4;
            *((uint32_t *)programStart) = 0xf33d393d;
            if (programLength < 0x100) {
                // 60xx8060093d393df3<>
                programStart -= 3;
                *((uint32_t *)programStart) = 0x3d096080;
                programStart -= 2;
                *((uint16_t *)programStart) = 0x0060 | programLength << 8;
                programLength += 9;
            } else if (programLength < 0x10000) {
                // 61xxxx80600a3d393df3<>
                programStart -= 3;
                *((uint32_t *)programStart) = 0x3d0a6080;
                programStart -= 3;
                *((uint32_t *)programStart) = 0x80000061 | (programLength & 0xff) << 16 | (programLength & 0xff00);
                programLength += 10;
            }
        }
    }

    for (; programLength--;) printf("%02x", *programStart++);
}

void disassemble(const char *contents) {
    disassembleInit();
    while (disassembleValid(&contents)) {
        disassembleNextOp(&contents);
    }
    disassembleFinalize();
}

int main(int argc, char *const argv[]) {
    scanInit();
    int option;
    while ((option = getopt (argc, argv, "cd")) != -1)
        switch (option) {
            case 'c':
                wrapConstructor = 1;
                break;
            case 'd':
                inverse = 1;
                break;
            case '?':
                fprintf(stderr, "Unknown evm option `-%c'.\n", optopt);
                return 1;
            default:
                return 1;
        }
    if (inverse && wrapConstructor) {
        fputs("-c cannot be used with -d\n", stderr);
        return 1;
    }
    if (optind == argc) {
        // TODO read from stdin
    }
    for (int i = optind; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            perror(argv[i]);
            _exit(1);
        }

        struct stat fstatus;
        int fstatSuccess = fstat(fd, &fstatus);
        if (fstatSuccess == -1) {
            perror(argv[i]);
            _exit(1);
        }
        
        const char *contents = mmap(NULL, fstatus.st_size, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);
        void *start = (void *)contents;
        if (contents == NULL) {
            perror(argv[i]);
        }
        if (inverse) {
            // TODO
            disassemble(contents);
        } else {
            assemble(contents);
        }
        munmap(start, fstatus.st_size);
        close(fd);
        putchar('\n');
    }
    return 0;
}




