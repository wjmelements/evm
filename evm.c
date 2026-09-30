#include "dio.h"
#include "network.h"
#include "path.h"
#include "scan.h"
#include "disassemble.h"
#include "json.h"
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
static int trace = 0;
static const char *traceFileName = NULL;
static uint64_t debugFlags = 0;

static void assemble(const char *contents) {
    op_t *programStart = &ops[CONSTRUCTOR_OFFSET];
    uint32_t programLength = 0;
    scanInit();
    for (; scanValid(&contents); programLength++) {
        if (programLength > (PROGRAM_BUFFER_LENGTH - CONSTRUCTOR_OFFSET)) {
            fputs("Program size exceeds limit; terminating", stderr);
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

    for (; programLength--;) {
        printf("%02x", *programStart++);
    }
    putchar('\n');
}

static void disassemble(const char *contents) {
    disassembleInit();
    while (disassembleValid(&contents)) {
        disassembleNextOp(&contents);
    }
    disassembleFinalize();
}

// The hex chars of the JSON string from val to end (just past its closing quote), without "0x"
static const char *jsonHex(const char *val, const char *end, const char *key, size_t *len) {
    if (*val != '"' || end[-1] != '"') {
        fprintf(stderr, "evm: \"%s\" must be a string\n", key);
        exit(1);
    }
    val++;
    end--;
    if (val[0] == '0' && val[1] == 'x') {
        val += 2;
    }
    *len = end - val;
    return val;
}

static address_t jsonAddress(const char *val, const char *end, const char *key) {
    size_t len;
    const char *hex = jsonHex(val, end, key, &len);
    if (len != 40) {
        fprintf(stderr, "evm: malformed \"%s\" address\n", key);
        exit(1);
    }
    return AddressFromHex40(hex);
}

static void execute(const char *contents) {
    address_t from = {{0}};
    address_t to;
    int hasTo = 0;
    const char *hexData = contents;
    size_t hexLen;
    val_t value = {0, 0, 0};
    bool hasNonce = false;
    uint64_t nonce = 0;
    block_t overrides;
    blockFields_t overridden = 0;

    if (contents[0] == '{') {
        hexData = "";
        hexLen = 0;
        const char *key, *val;
        size_t klen;
        for (const char *end = contents; (end = jNextKeyVal(end, &key, &klen, &val)); ) {
            const char *hex;
            size_t len;
            switch (klen) {
            case 2:
                if (!memcmp(key, "to", 2)) {
                    hasTo = 1;
                    to = jsonAddress(val, end, "to");
                }
                break;
            case 4:
                if (!memcmp(key, "from", 4)) {
                    from = jsonAddress(val, end, "from");
                } else if (!memcmp(key, "data", 4)) {
                    hexData = jsonHex(val, end, "data", &hexLen);
                }
                break;
            case 5:
                if (!memcmp(key, "input", 5)) {
                    hexData = jsonHex(val, end, "input", &hexLen);
                } else if (!memcmp(key, "value", 5)) {
                    hex = jsonHex(val, end, "value", &len);
                    for (size_t i = 0; i < len; i++) {
                        value[0] = (value[0] << 4) | (value[1] >> 28);
                        value[1] = (value[1] << 4) | (value[2] >> 28);
                        value[2] = (value[2] << 4) | hexString8ToUint8(hex[i]);
                    }
                } else if (!memcmp(key, "nonce", 5)) {
                    hex = jsonHex(val, end, "nonce", &len);
                    hasNonce = true;
                    for (size_t i = 0; i < len; i++) {
                        nonce = (nonce << 4) | hexString8ToUint8(hex[i]);
                    }
                }
                break;
            case 7:
                if (!memcmp(key, "chainId", 7)) {
                    hex = jsonHex(val, end, "chainId", &len);
                    blockParseField(&overrides, BLOCK_chainId_INDEX, hex, len);
                    overridden |= BLOCK_BIT(chainId);
                }
                break;
            case 14:
                if (!memcmp(key, "blockOverrides", 14)) {
                    const char *okey, *oval;
                    size_t oklen;
                    for (const char *oend = val; (oend = jNextKeyVal(oend, &okey, &oklen, &oval)); ) {
                        uint8_t index = blockKeyIndex(blockOverrideKey, okey, oklen);
                        if (index == BLOCK_FIELD_COUNT || index == BLOCK_chainId_INDEX) {
                            fprintf(stderr, "evm: unsupported blockOverrides key \"%.*s\"\n", (int)oklen, okey);
                            exit(1);
                        }
                        hex = jsonHex(oval, oend, blockOverrideKey[index], &len);
                        blockParseField(&overrides, index, hex, len);
                        overridden |= (blockFields_t)1 << index;
                    }
                }
                break;
            }
        }
        if (hexLen & 1) {
            fputs("evm: odd-lengthed input\n", stderr);
            exit(1);
        }
    } else {
        hexLen = strlen(contents);
        if (hexLen & 1 && contents[hexLen - 1] != '\n') {
            fputs("odd-lengthed input", stderr);
            exit(1);
        }
        if (hexLen >= 2 && hexData[0] == '0' && hexData[1] == 'x') {
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

    if (overridden) {
        evmOverrideBlock(&overrides, overridden);
    }
    if (hasNonce) {
        evmMockNonce(from, nonce);
    }

    uint64_t gas = 0xffffffffffffffff;
    result_t result;
    if (hasTo) {
        result = txCall(from, gas, to, value, input, NULL);
    } else {
        result = txCreate(from, gas, value, input);
    }

    if (outputJson) {
        fputs("{\"", stdout);
        if (includeGas) {
            printf("gasUsed\":\"0x%" PRIx64 "\",\"", gas - result.gasRemaining);
        }
        if (includeLogs) {
            fputs("logs\":", stdout);
            fprintLogs(stdout, result.stateChanges, true);
            fputs(",\"", stdout);
        }
        if (includeStatus) {
            fputs("status\":\"", stdout);
            if (!hasTo && !zero256(&result.status)) {
                // CREATE status is a deployed address: print it fixed-width, not compact.
                fprintAddress(stdout, AddressFromUint256(&result.status));
            } else {
                fprintCompact256(stdout, &result.status);
            }
            fputs("\",\"", stdout);
        }
        block_t used;
        blockFields_t usedFields = evmBlockUsed(&used);
        if (usedFields & BLOCK_BIT(chainId)) {
            fputs("chainId\":", stdout);
            fprintBlockField(stdout, &used, BLOCK_chainId_INDEX);
            fputs(",\"", stdout);
            usedFields &= ~BLOCK_BIT(chainId);
        }
        if (usedFields) {
            fputs("blockOverrides\":{", stdout);
            const char *separator = "\"";
            for (uint8_t index = 0; index < BLOCK_FIELD_COUNT; index++) {
                if (usedFields & ((blockFields_t)1 << index)) {
                    printf("%s%s\":", separator, blockOverrideKey[index]);
                    fprintBlockField(stdout, &used, index);
                    separator = ",\"";
                }
            }
            fputs("},\"", stdout);
        }
        fputs("returnData\":\"0x", stdout);
    }
    for (; result.returnData.size--;) {
        printf("%02x", *result.returnData.content++);
    }
    if (outputJson) {
        fputs("\"}", stdout);
    }
    putchar('\n');
    fflush(stdout);
}

#define USAGE fputs("usage: evm [ [-w json-file [-u] ] [-x [-n] [-gls] ] [-D flags | -t] [-T trace-file] | [-c | -C] [-j] | -d ] [-o input] [file...]\n", stderr)

static const struct option long_options[] = {
    {"version", no_argument, NULL, 'v'},
    {0, 0, 0, 0},
};

int main(int argc, char *const argv[]) {
    pathInit(argv[0]);

    int option;
    char *contents = NULL;
    const char **configFiles = calloc(argc - 1, sizeof(char *));
    int configCount = 0;
    while ((option = getopt_long(argc, argv, "cCdD:gjlo:nstT:uvw:x", long_options, NULL)) != -1) {
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
        case 'D': {
            char *end;
            debugFlags = strtoull(optarg, &end, 16);
            if (*end || end == optarg) {
                fprintf(stderr, "evm: malformed debug flags %s\n", optarg);
                return 1;
            }
            break;
        }
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
        case 't':
            trace = 1;
            break;
        case 'T':
            traceFileName = optarg;
            break;
        case 'u':
            updateConfigFile = 1;
            break;
        case 'v':
            puts(evm_build_version);
            return 0;
        case 'w':
            configFile = optarg;
            configFiles[configCount++] = optarg;
            break;
        case '?':
        default:
            USAGE;
            return 1;
        }
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
    if (trace && debugFlags) {
        fputs("-D cannot be used with -t\n", stderr);
        USAGE;
        return 1;
    }
    if (debugFlags && !runtime && !configFile) {
        fputs("-D requires -x or -w\n", stderr);
        USAGE;
        return 1;
    }
    if (trace && !runtime && !configFile) {
        fputs("-t requires -x or -w\n", stderr);
        USAGE;
        return 1;
    }
    if (traceFileName) {
        int traceFd = open(traceFileName, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (traceFd < 0) {
            perror(traceFileName);
            return 1;
        }
        evmSetDebugFile(traceFd);
    }
    evmSetTrace(trace);
    evmSetDebug(debugFlags);
    setConfigDebug(debugFlags);
    if (configCount) {
        evmInit();
        for (int i = 0; i < configCount; i++) {
            loadConfig(configFiles[i], updateConfigFile);
        }
    }
    free(configFiles);
    evmSetDebug(debugFlags);
    void (*subprogram)(const char*);
    if (inverse) {
        subprogram = disassemble;
    } else if (runtime) {
        subprogram = execute;
    } else if (configFile) {
        // tests should exit(1) if they fail
        exit(0);
    } else {
        subprogram = assemble;
    }
    if (runtime && configFile == NULL) {
        if (networkMode) {
            evmSetNetworkFetch();
        }
        evmInit();
    }
    if (contents != NULL) {
        // input is from the command line
        subprogram(contents);
    } else if (optind == argc) {
        if (runtime) {
            // line-by-line: each JSON object is a separate call sharing EVM state
            char *line = NULL;
            size_t cap = 0;
            ssize_t len;
            while ((len = getline(&line, &cap, stdin)) != -1) {
                if (len > 0 && line[len - 1] == '\n') {
                    line[--len] = '\0';
                }
                if (len > 0) {
                    subprogram(line);
                }
            }
            free(line);
        } else {
            // read from stdin as one blob (assemble / disassemble)
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
        }
    } else {
        for (int i = optind; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd == -1) {
                perror(argv[i]);
                exit(1);
            }

            struct stat fstatus;
            int fstatSuccess = fstat(fd, &fstatus);
            if (fstatSuccess == -1) {
                perror(argv[i]);
                exit(1);
            }

            contents = mmap(NULL, fstatus.st_size, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);
            if (contents == NULL) {
                perror(argv[i]);
            }
            subprogram(contents);
            munmap(contents, fstatus.st_size);
            close(fd);
        }
    }
    return 0;
}
