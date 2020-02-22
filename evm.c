#include "scan.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define CONSTRUCTOR_OFFSET 0x1000
#define PROGRAM_BUFFER_LENGTH 0x8000
op_t ops[PROGRAM_BUFFER_LENGTH];


int main(int argc, char *const argv[]) {
    scanInit();
    int option;
    int wrapConstructor = 0;
    while ((option = getopt (argc, argv, "c")) != -1)
        switch (option) {
            case 'c':
                wrapConstructor = 1;
                break;
            case '?':
                fprintf(stderr, "Unknown evm option `-%c'.\n", optopt);
                return 1;
            default:
                return 1;
        }
    for (int i = optind; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            perror(argv[i]);
            return 1;
        }

        struct stat fstatus;
        int fstatSuccess = fstat(fd, &fstatus);
        if (fstatSuccess == -1) {
            perror(argv[i]);
            return 1;
        }
        
        const char *contents = mmap(NULL, fstatus.st_size, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);
        void *start = (void *)contents;
        if (contents == NULL) {
            perror(argv[i]);
        }
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
            programStart -= 4;
            *((uint32_t *)programStart) = 0xf33d393d;
            if (programLength < 0x100) {
                // 60xx8060093d393df3
                programStart -= 3;
                *((uint32_t *)programStart) = 0x3d096080;
                programStart -= 2;
                *((uint16_t *)programStart) = 0x0060 | programLength << 8;
                programLength += 9;
            } else if (programLength < 0x10000) {
                // 61xxxx80600a3d393df3
                programStart -= 3;
                *((uint32_t *)programStart) = 0x3d0a6080;
                programStart -= 3;
                *((uint32_t *)programStart) = 0x80000061 | (programLength & 0xff) << 16 | (programLength & 0xff00);
                programLength += 10;
            }
        }

        for (; programLength--;) printf("%02x", *programStart++);

        munmap(start, fstatus.st_size);
        close(fd);
        putchar('\n');
    }
    return 0;
}
