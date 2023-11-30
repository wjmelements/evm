#include "dio.h"
#include "scan.h"
#include "disassemble.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CONSTRUCTOR_OFFSET 0x1000
#define PROGRAM_BUFFER_LENGTH 0x8000
op_t ops[PROGRAM_BUFFER_LENGTH];


static int wrapConstructor = 0;
static int inverse = 0;
static int runtime = 0;
#define outputJson (includeGas || includeStatus | includeLogs)
static int includeGas = 0;
static int includeStatus = 0;
static int includeLogs = 0;
static const char *configFile = NULL;

static void assemble(const char *contents) {
    op_t *programStart = &ops[CONSTRUCTOR_OFFSET];
    uint32_t programLength = 0;
    scanInit();
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
    putchar('\n');
}

static void disassemble(const char *contents) {
    disassembleInit();
    while (disassembleValid(&contents)) {
        disassembleNextOp(&contents);
    }
    disassembleFinalize();
}

static char *selfPath;

static void runConfig(const char *_configFile) {
    if (_configFile != NULL) {
        int fd = open(_configFile, O_RDONLY);
        if (fd == -1) {
            perror(_configFile);
            _exit(1);
        }

        struct stat fstatus;
        int fstatSuccess = fstat(fd, &fstatus);
        if (fstatSuccess == -1) {
            perror(_configFile);
            _exit(1);
        }
        char *configContents = mmap(NULL, fstatus.st_size, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);
        dioInit(selfPath);
        applyConfig(configContents);
        munmap(configContents, fstatus.st_size);
        close(fd);
    }
}

static void execute(const char *contents) {
    if (configFile == NULL) {
        evmInit();
    }
    size_t len = strlen(contents);
    if (len & 1 && contents[len - 1] != '\n') {
        fputs("odd-lengthed input", stderr);
        _exit(1);
    }
    if (len > 2 && contents[0] == '0' && contents[1] == 'x') {
        // allow 0x prefix
        len -= 2;
        contents += 2;
    }
    data_t input;
    input.size = len / 2;
    input.content = malloc(input.size);

    for (size_t i = 0; i < input.size; i++) {
        input.content[i] = hexString16ToUint8(contents + i * 2);
    }

    // TODO support these eth_call parameters
    address_t from;
    uint64_t gas = 0xffffffffffffffff;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;
    result_t result;
    if (false) {
        address_t to; // TODO support this parameter
        result = txCall(from, gas, to, value, input, NULL);
    } else {
        result = txCreate(from, gas, value, input);
    }
    evmFinalize();

    if (outputJson) {
        fputs("{\"", stdout);
        if (includeGas) {
            printf("gasUsed\":%llu,\"", gas - result.gasRemaining);
        }
        if (includeLogs) {
            fputs("logs\":", stdout);
            fprintLogs(stdout, result.stateChanges);
            fputs(",\"", stdout);
        }
        if (includeStatus) {
            printf(
                "status\":\"0x%08llx%08llx%08llx%08llx\",\"",
                UPPER(UPPER(result.status)),
                LOWER(UPPER(result.status)),
                UPPER(LOWER(result.status)),
                LOWER(LOWER(result.status))
            );
        }
        fputs("returnData\":\"0x", stdout);
    }
    for (;result.returnData.size--;) printf("%02x", *result.returnData.content++);
    if (outputJson) {
        fputs("\"}", stdout);
    }
    putchar('\n');

}

#define USAGE fputs("usage: evm [ [-x [-g] [-s] ] | [-c] | [-d] ] [-o input] [file...]\n", stderr)

int main(int argc, char *const argv[]) {
    selfPath = argv[0];
    int option;
    char *contents = NULL;
    while ((option = getopt (argc, argv, "cdglo:sw:x")) != -1)
        switch (option) {
            case 'c':
                wrapConstructor = 1;
                break;
            case 'd':
                inverse = 1;
                break;
            case 'o':
                contents = optarg;
                break;
            case 'x':
                runtime = 1;
                break;
            case 'g':
                includeGas = 1;
                break;
            case 's':
                includeStatus = 1;
                break;
            case 'l':
                includeLogs = 1;
                break;
            case 'w':
                if (configFile == NULL) {
                    evmInit();
                }
                configFile = optarg;
                runConfig(configFile);
                break;
            case '?':
            default:
                USAGE;
                return 1;
        }
    if (inverse && wrapConstructor) {
        fputs("-c cannot be used with -d\n", stderr);
        USAGE;
        return 1;
    }
    if (runtime && wrapConstructor) {
        fputs("-c cannot be used with -x\n", stderr);
        USAGE;
        return 1;
    }
    if (inverse && runtime) {
        fputs("-d cannot be used with -x\n", stderr);
        USAGE;
        return 1;
    }
    void (*subprogram)(const char*);
    if (inverse) {
        subprogram = disassemble;
    } else if (runtime) {
        subprogram = execute;
    } else if (configFile) {
        // tests should _exit(1) if they fail
        _exit(0);
    } else {
        subprogram = assemble;
    }
    if (contents != NULL) {
        // input is from the command line
        subprogram(contents);
    } else if (optind == argc) {
        // read from stdin
        size_t bufferSize = 4;
        size_t capacity = bufferSize - 1;
        char *input = calloc(1, bufferSize);
        char *pos = input;
        while (1) {
            ssize_t red = read(0, pos, capacity);
            if (red == -1) {
                perror("stdin");
                return 1;
            }
            if (red == 0) {
                // EOF
                break;
            }
            capacity -= red;
            if (capacity) {
                pos += red;
            } else {
                char *next = calloc(1, bufferSize << 1);
                memcpy(next, input, bufferSize);
                pos = next + bufferSize - 1;
                capacity = bufferSize;
                bufferSize <<= 1;
                free(input);
                input = next;
            }
        }
        subprogram(input);
        // free is redundant with program termination but makes valgrind happy
        free(input);
    } else for (int i = optind; i < argc; i++) {
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
        
        contents = mmap(NULL, fstatus.st_size, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);
        if (contents == NULL) {
            perror(argv[i]);
        }
        subprogram(contents);
        munmap(contents, fstatus.st_size);
        close(fd);
    }
    return 0;
}
