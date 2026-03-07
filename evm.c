#include "dio.h"
#include "network.h"
#include "path.h"
#include "scan.h"
#include "disassemble.h"
#include "version.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CONSTRUCTOR_OFFSET 0x1000
#define PROGRAM_BUFFER_LENGTH 0x8000
op_t ops[PROGRAM_BUFFER_LENGTH];


static int wrapUniversalConstructor = 0;
static int wrapMinConstructor = 0;
static int labelJumpdests = 0;
static int inverse = 0;
static int runtime = 0;
#define outputJson (includeGas || includeStatus | includeLogs)
static int includeGas = 0;
static int includeStatus = 0;
static int includeLogs = 0;
static const char *configFile = NULL;
static int updateConfigFile = 0;
static int networkMode = 0;

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
    if (labelJumpdests) {
        fprintLabels(stdout);
        return;
    }
    if (wrapMinConstructor) {
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
    } else if (wrapUniversalConstructor) {
        // 600b380380600b3d393df3<>
        programStart -= 4;
        *((uint32_t *)programStart) = 0xf33d393d;
        programStart -= 4;
        *((uint32_t *)programStart) = 0x0b608003;
        programStart -= 3;
        *((uint32_t *)programStart) = 0x03380b60;
        programLength += 11;
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

// Scan json for "key":"<value>". Strips leading "0x" if present.
// Returns pointer into json at start of hex chars, sets *len to char count.
// Returns NULL if the key is not found.
static const char *jsonStrVal(const char *json, const char *key, size_t *len) {
    size_t klen = strlen(key);
    const char *p = json;
    for (;;) {
        p = strchr(p, '"');
        if (!p) return NULL;
        if (strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
            p += 1 + klen + 1;
            while (*p == ' ' || *p == ':') p++;
            if (*p != '"') return NULL;
            p++;
            if (p[0] == '0' && p[1] == 'x') p += 2;
            const char *start = p;
            while (*p && *p != '"') p++;
            *len = (size_t)(p - start);
            return start;
        }
        p++;
    }
}

static void execute(const char *contents) {
    if (configFile == NULL) {
        evmInit();
    }
    if (networkMode) {
        evmSetNetworkFetch();
    }

    address_t from = {{0}};
    address_t to;
    int hasTo = 0;
    const char *hexData = contents;
    size_t hexLen;

    if (contents[0] == '{') {
        size_t flen;
        const char *p = jsonStrVal(contents, "to", &flen);
        if (p && flen == 40) { hasTo = 1; to = AddressFromHex40(p); }
        p = jsonStrVal(contents, "from", &flen);
        if (p && flen == 40) from = AddressFromHex40(p);
        p = jsonStrVal(contents, "data", &flen);
        if (!p) p = jsonStrVal(contents, "input", &flen);
        hexData = p ? p : "";
        hexLen = p ? flen : 0;
    } else {
        hexLen = strlen(contents);
        if (hexLen & 1 && contents[hexLen - 1] != '\n') {
            fputs("odd-lengthed input", stderr);
            _exit(1);
        }
        if (hexLen > 2 && hexData[0] == '0' && hexData[1] == 'x') {
            hexLen -= 2;
            hexData += 2;
        }
    }
    data_t input;
    input.size = hexLen / 2;
    input.content = input.size ? malloc(input.size) : NULL;
    for (size_t i = 0; i < input.size; i++) {
        input.content[i] = hexString16ToUint8(hexData + i * 2);
    }

    uint64_t gas = 0xffffffffffffffff;
    val_t value = {0, 0, 0};
    result_t result;
    if (hasTo) {
        result = txCall(from, gas, to, value, input, NULL);
    } else {
        result = txCreate(from, gas, value, input);
    }
    evmFinalize();

    if (networkMode) {
        fputs("{\"output\":\"0x", stdout);
        for (size_t i = 0; i < result.returnData.size; i++)
            printf("%02x", result.returnData.content[i]);
        fputs("\"}\n", stdout);
    } else if (outputJson) {
        fputs("{\"", stdout);
        if (includeGas) {
            printf("gasUsed\":%" PRIu64 ",\"", gas - result.gasRemaining);
        }
        if (includeLogs) {
            fputs("logs\":", stdout);
            fprintLogs(stdout, result.stateChanges, true);
            fputs(",\"", stdout);
        }
        if (includeStatus) {
            printf(
                "status\":\"0x%08" PRIx64 "%08" PRIx64 "%08" PRIx64 "%08" PRIx64 "\",\"",
                UPPER(UPPER(result.status)),
                LOWER(UPPER(result.status)),
                UPPER(LOWER(result.status)),
                LOWER(LOWER(result.status))
            );
        }
        fputs("returnData\":\"0x", stdout);
        for (;result.returnData.size--;) printf("%02x", *result.returnData.content++);
        fputs("\"}", stdout);
        putchar('\n');
    } else {
        for (;result.returnData.size--;) printf("%02x", *result.returnData.content++);
        putchar('\n');
    }

}

#define USAGE fputs("usage: evm [ [-w json-file [-u] ] [-x [-n] [-gs] ] | [-c | -C] [-j] | -d ] [-o input] [file...]\n", stderr)

static const struct option long_options[] = {
    {"version", no_argument, NULL, 'v'},
    {0, 0, 0, 0},
};

int main(int argc, char *const argv[]) {
    pathInit(argv[0]);

    int option;
    char *contents = NULL;
    while ((option = getopt_long(argc, argv, "cCdgjlo:nsuvw:x", long_options, NULL)) != -1)
        switch (option) {
            case 'c':
                wrapMinConstructor = 1;
                break;
            case 'C':
                wrapUniversalConstructor = 1;
                break;
            case 'd':
                inverse = 1;
                break;
            case 'j':
                labelJumpdests = 1;
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
            case 'n':
                networkMode = 1;
                break;
            case 's':
                includeStatus = 1;
                break;
            case 'l':
                includeLogs = 1;
                break;
            case 'u':
                updateConfigFile = 1;
                break;
            case 'v':
                puts(evm_build_version);
                return 0;
            case 'w':
                if (configFile == NULL) {
                    evmInit();
                }
                configFile = optarg;
                loadConfig(configFile, updateConfigFile);
                break;
            case '?':
            default:
                USAGE;
                return 1;
        }
    if (inverse && wrapMinConstructor) {
        fputs("-c cannot be used with -d\n", stderr);
        USAGE;
        return 1;
    }
    if (inverse && wrapUniversalConstructor) {
        fputs("-C cannot be used with -d\n", stderr);
        USAGE;
        return 1;
    }
    if (inverse && labelJumpdests) {
        fputs("-j cannot be used with -d\n", stderr);
        USAGE;
        return 1;
    }
    if (runtime && wrapMinConstructor) {
        fputs("-c cannot be used with -x\n", stderr);
        USAGE;
        return 1;
    }
    if (runtime && wrapUniversalConstructor) {
        fputs("-C cannot be used with -x\n", stderr);
        USAGE;
        return 1;
    }
    if (wrapMinConstructor && wrapUniversalConstructor) {
        fputs("-c cannot be used with -C\n", stderr);
        USAGE;
        return 1;
    }
    if (runtime && labelJumpdests) {
        fputs("-j cannot be used with -x\n", stderr);
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
