#include "scan.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, const char *argv[]) {
    scanInit();
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            perror(argv[i]);
        }

        struct stat fstatus;
        int fstatSuccess = fstat(fd, &fstatus);
        if (fstatSuccess == -1) {
            perror(argv[i]);
        }
        
        const char *contents = mmap(NULL, fstatus.st_size, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);
        void *start = (void *)contents;
        if (contents == NULL) {
            perror(argv[i]);
        }
        while (scanValid(&contents)) {
            printf("%02x", scanNextOp(&contents));
        }
        munmap(start, fstatus.st_size);
        close(fd);
        putchar('\n');
    }
}
