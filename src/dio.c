#include "dio.h"

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static inline int jsonIgnores(char ch) {
    return ch != '{'
        && ch != '}'
        && ch != '['
        && ch != ']'
        && ch != ','
        && ch != ':'
        && ch != '"'
        && (ch < '0' || ch > '9')
        && (ch < 'A' || ch > 'Z')
        && (ch < 'a' || ch > 'z')
    ;
}


static void jsonScanWaste(const char **iter) {
    for (; jsonIgnores(**iter); (*iter)++);
}

static void jsonScanChar(const char **iter, char expected) {
    for (char ch; (ch = **iter) != expected; (*iter)++) {
        if (!jsonIgnores(ch)) {
            fprintf(stderr, "Config: when seeking '%c' found unexpected character '%c'\n", expected, ch);
            assert(expected == ch);
        }
    }
    (*iter)++;
}

static void jsonSkipExpectedChar(const char **iter, char expected) {
    if (**iter == expected) {
        (*iter)++;
    } else {
        fprintf(stderr, "Config: expecting '%c', found '%c'\n", expected, **iter);
    }
}

static const char *jsonScanStr(const char **iter) {
    jsonScanChar(iter, '"');
    const char *start = *iter;
    for (char ch; (ch = **iter) != '"' && ch; (*iter)++);
    jsonSkipExpectedChar(iter, '"');
    return start;
}

typedef struct storageEntry {
    uint256_t key;
    uint256_t value;
    struct storageEntry *prev;
} storageEntry_t;

typedef struct testEntry {
    op_t op;
    data_t input;
    data_t output;
    struct testEntry *prev;
} testEntry_t;

typedef struct entry {
    address_t *address;
    val_t balance;
    uint64_t nonce;
    data_t code;
    storageEntry_t *storage;
    testEntry_t *tests;
    char *constructPath;
} entry_t;

static char *selfPath;
static char *derivedPath = NULL;
static int testFailure = 0;

void dioInit(char *_selfPath) {
    selfPath = _selfPath;
}

static void derivePath() {
    if (derivedPath != NULL) {
        return;
    }
    if (selfPath[0] == '/') {
        // absolute path
        derivedPath = selfPath;
        return;
    }
    const char *pathWalk = selfPath;
    char *relativePath = malloc(MAXPATHLEN + 1);
    while (*pathWalk) {
        if (*pathWalk == '/') {
            // relative path
            getwd(relativePath);
            size_t offset = strlen(relativePath);
            relativePath[offset++] = '/';
            relativePath[offset] = '\0';
            strcpy(relativePath + offset, selfPath);
            derivedPath = relativePath;
            return;
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
            return;
        }
    }
    fputs("evm: could not find evm\n", stderr);
    _exit(-1);
}

static void runTests(const entry_t *entry, testEntry_t *test) {
    if (test == NULL) {
        return;
    }
    runTests(entry, test->prev);

    // TODO Support these parameters
    address_t from;
    uint64_t gas = 0xffffffffffffffff;
    val_t value;
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;
    // TODO support evmStaticCall
    result_t result = evmCall(from, gas, *entry->address, value, test->input);

    if (LOWER(LOWER(result.status))) {
        fputs("Execution reverted\n", stderr);
        // TODO allow revert as expected status
        testFailure = 1;
    }

    if (result.returnData.size != test->output.size || memcmp(result.returnData.content, test->output.content, test->output.size)) {
        fputs("Output data mismatch\nactual:\n", stderr);

        for (size_t i = 0; i < result.returnData.size; i++) {
            fprintf(stderr, "%02x", result.returnData.content[i]);
        }
        fputs("\nexpected:\n", stderr);
        for (size_t i = 0; i < test->output.size; i++) {
            fprintf(stderr, "%02x", test->output.content[i]);
        }

        fputc('\n', stderr);
        testFailure = 1;
    }

    free(test);
}

static void applyEntry(entry_t *entry) {
    if (entry->address == NULL) {
        entry->address = calloc(1, sizeof(address_t));
        entry->address->address[0] = 0xaa;
        entry->address->address[1] = 0xbb;
        static uint32_t anonymousId;
        *(uint32_t *)(&entry->address->address[15]) = anonymousId++;
    }
    if (entry->constructPath) {
        int rw[2];
        if (pipe(rw) == -1) {
            perror("pipe");
            _exit(-1);
        }
        derivePath();
        //fprintf(stderr, "Loading from %s with %s\n", entry->constructPath, derivedPath);
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
                derivedPath,
                "-c",
                entry->constructPath,
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

        // TODO support these parameters
        address_t from;
        uint64_t gas = 0xffffffffffffffff;
        val_t value;
        value[0] = 0;
        value[1] = 0;
        value[2] = 0;

        result_t constructResult = evmConstruct(from, *entry->address, gas, value, input);
        if (entry->code.size) {
            // verify result code matches entry->code if present
            if (constructResult.returnData.size != entry->code.size || memcmp(constructResult.returnData.content, entry->code.content, entry->code.size)) {
                fputs("Code mismatch at address ", stderr);
                fprintAddress(stderr, (*entry->address));
                fprintf(stderr, ":\n`%s -c %s`:\n", derivedPath, entry->constructPath);
                for (size_t i = 0; i < constructResult.returnData.size; i++) {
                    fprintf(stderr, "%02x", constructResult.returnData.content[i]);
                }
                fprintf(stderr, "\nexpected:\n");
                for (size_t i = 0; i < entry->code.size; i++) {
                    fprintf(stderr, "%02x", entry->code.content[i]);
                }
                fputc('\n', stderr);
                _exit(-1);
            }
        }
    } else if (entry->code.size) {
        evmMockCode(*entry->address, entry->code);
    }
    evmMockNonce(*entry->address, entry->nonce);
    evmMockBalance(*entry->address, entry->balance);
    while (entry->storage != NULL) {
        storageEntry_t *prev = entry->storage;
        evmMockStorage(*entry->address, &prev->key, &prev->value);
        entry->storage = prev->prev;
        free(prev);
    }
    runTests(entry, entry->tests);
}

static void jsonScanEntry(const char **iter) {
    entry_t entry;
    bzero(&entry, sizeof(entry_t));
    jsonScanChar(iter, '{');
    do {
        const char *heading = jsonScanStr(iter);
        jsonScanChar(iter, ':');
        jsonScanWaste(iter);
        switch(*(uint32_t *)heading) {
            case 'rdda':
                // address
                jsonSkipExpectedChar(iter, '"');
                jsonSkipExpectedChar(iter, '0');
                jsonSkipExpectedChar(iter, 'x');
                entry.address = malloc(sizeof(address_t));
                for (unsigned int i = 0; i < 20; i++) {
                    entry.address->address[i] = hexString16ToUint8(*iter);
                    (*iter) += 2;
                }
                jsonSkipExpectedChar(iter, '"');
                break;
            case 'alab':
                // balance
                // TODO support decimal integer
                jsonSkipExpectedChar(iter, '"');
                jsonSkipExpectedChar(iter, '0');
                jsonSkipExpectedChar(iter, 'x');
                while (**iter != '"') {
                    entry.balance[0] <<= 4;
                    entry.balance[0] |= entry.balance[1] >> 28;
                    entry.balance[1] <<= 4;
                    entry.balance[1] |= entry.balance[2] >> 28;
                    entry.balance[2] <<= 4;
                    entry.balance[2] |= hexString8ToUint8(**iter);
                    (*iter)++;
                }
                jsonSkipExpectedChar(iter, '"');
                break;
            case 'cnon':
                // nonce
                // TODO support decimal integer
                jsonSkipExpectedChar(iter, '"');
                jsonSkipExpectedChar(iter, '0');
                jsonSkipExpectedChar(iter, 'x');
                while (**iter != '"') {
                    entry.nonce <<= 4;
                    entry.nonce |= hexString8ToUint8(**iter);
                    (*iter)++;
                }
                jsonSkipExpectedChar(iter, '"');
                break;
            case 'edoc':
                // code
                {
                    const char *start = jsonScanStr(iter);
                    jsonSkipExpectedChar(&start, '0');
                    jsonSkipExpectedChar(&start, 'x');
                    entry.code.size = (*iter - start) / 2;
                    entry.code.content = calloc(entry.code.size, 1);
                    for (unsigned int i = 0; i < entry.code.size; i++) {
                        entry.code.content[i] = hexString16ToUint8(start);
                        start += 2;
                    }
                }
                break;
            case 'snoc':
                // construct
                {
                    const char *start = jsonScanStr(iter);
                    size_t len = *iter - start - 1;
                    entry.constructPath = malloc(len + 1);
                    memcpy(entry.constructPath, start, len);
                    entry.constructPath[len] = '\0';
                }
                break;
            case 'tset':
                // tests
                {
                    jsonSkipExpectedChar(iter, '[');
                    jsonScanWaste(iter);
                    if (**iter != ']') do {
                        jsonScanChar(iter, '{');
                        jsonScanWaste(iter);

                        testEntry_t *test = calloc(1, sizeof(testEntry_t));
                        test->prev = entry.tests;
                        entry.tests = test;

                        if (**iter != '}') do {
                            const char *testHeading = jsonScanStr(iter);
                            size_t testHeadingLen = *iter - testHeading - 1;
                            jsonScanChar(iter, ':');
                            const char *testValue = jsonScanStr(iter);
                            size_t testValueLength = *iter - testValue - 1;
                            if (testHeadingLen == 5 && *testHeading == 'i') {
                                // input
                                jsonSkipExpectedChar(&testValue, '0');
                                jsonSkipExpectedChar(&testValue, 'x');
                                test->input.size = (testValueLength - 2) / 2;
                                test->input.content = malloc(test->input.size);
                                for (size_t i = 0; i < test->input.size; i++) {
                                    test->input.content[i] = hexString16ToUint8(testValue + i * 2);
                                }
                            } else if (testHeadingLen == 6 && *testHeading == 'o') {
                                // output
                                jsonSkipExpectedChar(&testValue, '0');
                                jsonSkipExpectedChar(&testValue, 'x');
                                test->output.size = (testValueLength - 2) / 2;
                                test->output.content = malloc(test->output.size);
                                for (size_t i = 0; i < test->output.size; i++) {
                                    test->output.content[i] = hexString16ToUint8(testValue + i * 2);
                                }
                            } else if (testHeadingLen == 2 && *testHeading == 'o') {
                                const char *end;
                                test->op = parseOp(testValue, &end);
                            } else {
                                fputs("Unexpected test heading: ", stderr);
                                for (size_t i = 0; i < testHeadingLen; i++) {
                                    fputc(testHeading[i], stderr);
                                }
                                fputc('\n', stderr);
                            }
                            jsonScanWaste(iter);
                            if (**iter == ',') {
                                jsonSkipExpectedChar(iter, ',');
                                jsonScanWaste(iter);
                                continue;
                            } else {
                                break;
                            }
                        } while (1);

                        jsonSkipExpectedChar(iter, '}');
                        if (**iter == ',') {
                            jsonSkipExpectedChar(iter, ',');
                            jsonScanWaste(iter);
                            continue;
                        } else {
                            break;
                        }
                    } while (1);
                    jsonScanChar(iter, ']');
                }
                break;
            case 'rots':
                // storage
                jsonScanChar(iter, '{');
                do {
                    storageEntry_t *storageEntry = malloc(sizeof(storageEntry_t));
                    storageEntry->prev = entry.storage;
                    entry.storage = storageEntry;
                    jsonScanChar(iter, '"');
                    jsonSkipExpectedChar(iter, '0');
                    jsonSkipExpectedChar(iter, 'x');
                    clear256(&storageEntry->key);
                    while (**iter != '"') {
                        shiftl256(&storageEntry->key, 4, &storageEntry->key);
                        LOWER(LOWER(storageEntry->key)) |= hexString8ToUint8(**iter);
                        (*iter)++;
                    }
                    jsonSkipExpectedChar(iter, '"');
                    jsonScanChar(iter, ':');
                    jsonScanChar(iter, '"');
                    jsonSkipExpectedChar(iter, '0');
                    jsonSkipExpectedChar(iter, 'x');
                    clear256(&storageEntry->value);
                    while (**iter != '"') {
                        shiftl256(&storageEntry->value, 4, &storageEntry->value);
                        LOWER(LOWER(storageEntry->value)) |= hexString8ToUint8(**iter);
                        (*iter)++;
                    }
                    jsonSkipExpectedChar(iter, '"');
                    if (**iter == ',') {
                        jsonSkipExpectedChar(iter, ',');
                        jsonScanWaste(iter);
                        continue;
                    } else {
                        break;
                    }
                } while (1);
                jsonScanChar(iter, '}');
                break;
            default:
                fprintf(stderr, "Unexpected entry heading: %04x\n", *(uint32_t *)heading); break;
        }
        jsonScanWaste(iter);
        if (**iter == ',') {
            jsonSkipExpectedChar(iter, ',');
            jsonScanWaste(iter);
            continue;
        } else {
            break;
        }
    } while (1);
    jsonScanChar(iter, '}');

    applyEntry(&entry);
}

void applyConfig(const char *json) {
    jsonScanChar(&json, '[');
    do {
        jsonScanEntry(&json);
        jsonScanWaste(&json);
        if (*json == ',') {
            jsonSkipExpectedChar(&json, ',');
            jsonScanWaste(&json);
            continue;
        } else {
            break;
        }
    } while (1);
    jsonScanChar(&json, ']');
    if (testFailure) {
        _exit(1);
    }
}
