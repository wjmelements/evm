#include "dio.h"
#include "path.h"
#include "vector.h"

#include <fcntl.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>


#define MAX_LOG_TOPICS 4

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

static uint64_t lineNumber = 0;

static void jsonScanWaste(const char **iter) {
    for (; jsonIgnores(**iter); (*iter)++) {
        if (**iter == '\n') {
            lineNumber++;
        }
    }
}

static void jsonScanChar(const char **iter, char expected) {
    for (char ch; (ch = **iter) != expected; (*iter)++) {
        if (jsonIgnores(ch)) {
            if (ch == '\n') {
                lineNumber++;
            }
        } else {
            fprintf(stderr, "Config: when seeking '%c' found unexpected character '%c' on line %" PRIu64 "\n", expected, ch, lineNumber);
            _exit(1);
        }
    }
    (*iter)++;
}

static void jsonFailExpectingChar(char expected, char actual) {
    fprintf(stderr, "Config: expecting '%c', found '%c' on line %" PRIu64 "\n", expected, actual, lineNumber);
    _exit(1);
}

static void jsonSkipExpectedChar(const char **iter, char expected) {
    if (**iter == expected) {
        (*iter)++;
    } else {
        jsonFailExpectingChar(expected, **iter);
    }
}

static const char *jsonScanStr(const char **iter) {
    jsonScanChar(iter, '"');
    const char *start = *iter;
    for (char ch; (ch = **iter) != '"' && ch; (*iter)++);
    jsonSkipExpectedChar(iter, '"');
    return start;
}

// attempt to skip entry of unknown json type
// ends at ',' or '}'
static void jsonSkipEntryValue(const char **iter) {
    jsonScanWaste(iter);
    char stack[100];
    bzero(stack, sizeof(stack));
    char *top = stack;
    *top = ',';
    (*iter)--;
    while (top <= stack + 100) {
        (*iter)++;
        if (*top == ',') {
            if (**iter == ',' || **iter == '}') {
                return;
            }
        }
        if (**iter == '\n') {
            lineNumber++;
            continue;
        }
        if (*top == '"') {
            if (**iter == '"') {
                top--;
            }
            continue;
        } else switch (**iter) {
            case '[':
            case '{':
            case '"':
                *(++top) = **iter;
                continue;
            case ']':
                if (*top-- != '[') {
                    jsonFailExpectingChar('}', ']');
                }
                break;
            case '}':
                if (*top-- != '{') {
                    jsonFailExpectingChar(']', '}');
                }
                break;
        }
    }
    fprintf(stderr, "Unexpected entry is too deep at line %" PRIu64 "\n", lineNumber);
    exit(1);
}

typedef struct storageEntry {
    uint256_t key;
    uint256_t value;
    struct storageEntry *prev;
} storageEntry_t;

typedef struct logsEntry {
    address_t address;
    logChanges_t *logs;
    struct logsEntry *prev;
} logsEntry_t;

typedef struct testResult {
    struct testResult *next;
    uint64_t gasUsed;
    const char *gasUsedBegin;
    const char *gasUsedEnd;
} testResult_t;

static struct testResults {
    testResult_t *head;
    testResult_t **tail;
} testResults;

typedef struct testEntry {
    char *name;
    op_t op;
    address_t from;
    address_t *to;
    val_t value;
    data_t input;
    data_t output;
    bool outputSpecified;
    uint64_t gas;
    accessList_t *accessList;
    uint256_t status;
    uint64_t gasUsed;
    logsEntry_t *logs;
    uint64_t *blockNumber;
    uint64_t *timestamp;
    uint64_t debug;
    testResult_t result;

    struct testEntry *prev;
} testEntry_t;

typedef struct entry {
    address_t *address;
    val_t balance;
    uint64_t nonce;
    address_t *creator;
    data_t initCode;
    data_t code;
    storageEntry_t *storage;
    testEntry_t *tests;
    char *path;
    char *importPath;
} entry_t;

static int anyTestFailure = 0;

static uint64_t runTests(const entry_t *entry, testEntry_t *test) {
    if (test == NULL) {
        return 0;
    }
    uint64_t testsRun = runTests(entry, test->prev);
    if (!testsRun) {
        fputs("# ", stderr);
        if (entry->path) {
            fputs(entry->path, stderr);
        } else {
            fprintAddress(stderr, (*entry->address));
        }
        fputc('\n', stderr);
    }

    evmSetDebug(test->debug);
    if (test->blockNumber) {
        evmSetBlockNumber(*test->blockNumber);
        free(test->blockNumber);
    }
    if (test->timestamp) {
        evmSetTimestamp(*test->timestamp);
        free(test->timestamp);
    }
    uint64_t gas = 0xffffffffffffffff;
    if (test->gas) {
        gas = test->gas;
    }
    // TODO support evmStaticCall
    result_t result = txCall(test->from, gas, test->to ? *test->to : *entry->address, test->value, test->input, test->accessList);
    uint64_t gasUsed = gas - result.gasRemaining;
    test->result.gasUsed = gasUsed;

    if (test->name) {
        fputs(test->name, stderr);
    } else {
        fprintf(stderr, "%" PRIu64, testsRun);
    }
    fputs(": ", stderr);
    int testFailure = 0;
    if (!equal256(&result.status, &test->status) || test->op == CREATE) {
        if (zero256(&result.status)) {
            if (result.returnData.size) {
                fputs("\033[0;31mreverted: ", stderr);
                fprintData(stderr, result.returnData);
                fputs("\033[0m\n", stderr);
            } else {
                fputs("\033[0;31mreverted\033[0m\n", stderr);
            }
        } else {
            fputs("\033[0;31mshould revert\033[0m\n", stderr);
        }
        testFailure = anyTestFailure = 1;
    } else {
        const logsEntry_t *expectedLogs = test->logs;
        while (expectedLogs) {
            // find corresponding acccount
            const stateChanges_t *actualLogs = result.stateChanges;
            while (actualLogs) {
                if (AddressEqual(&actualLogs->account, &expectedLogs->address)) {
                    break;
                }
                actualLogs = actualLogs->next;
            }
            if (actualLogs) {
                // compare all logs
                const logChanges_t *expectedLog = expectedLogs->logs;
                const logChanges_t *actualLog = actualLogs->logChanges;
                if (!LogsEqual(expectedLog, actualLog)) {
                    // mismatch
                    fputs("logs expected:\n", stderr);
                    fprintLog(stderr, expectedLog, false);
                    fputc('\n', stderr);
                    fputs("logs actual:\n", stderr);
                    fprintLogDiff(stderr, actualLog, expectedLog, false);
                    fputc('\n', stderr);
                    testFailure = anyTestFailure = 1;
                }
            } else {
                // Missing!
                fputs("expected logs missing:\n", stderr);
                fprintLog(stderr, expectedLogs->logs, false);
                fputc('\n', stderr);
                testFailure = anyTestFailure = 1;
            }
            expectedLogs = expectedLogs->prev;
        }
    }

    if (test->outputSpecified && (result.returnData.size != test->output.size || memcmp(result.returnData.content, test->output.content, test->output.size))) {
        fputs("Output data mismatch\nactual:\n", stderr);

        fprintData(stderr, result.returnData);
        fputs("\nexpected:\n", stderr);
        fprintData(stderr, test->output);
        fputc('\n', stderr);
        testFailure = anyTestFailure = 1;
    } else if (test->gasUsed) {
        if (test->gasUsed < gasUsed) {
            // more actual gasUsed than expected
            fprintf(stderr, "gasUsed \033[0;31m%" PRIu64 "\033[0m expected %" PRIu64 " (\033[0;31m+%" PRIu64 "\033[0m)\n", gasUsed, test->gasUsed, gasUsed - test->gasUsed);
        } else if (test->gasUsed > gas - result.gasRemaining) {
            // less actual gasUsed than expected
            fprintf(stderr, "gasUsed \033[0;32m%" PRIu64 "\033[0m expected %" PRIu64 " (\033[0;32m-%" PRIu64 "\033[0m)\n", gasUsed, test->gasUsed, test->gasUsed - gasUsed);
        } else if (testFailure) {
            fprintf(stderr, "\033[0;31mfail\033[0m\n");
        } else {
            fprintf(stderr, "\033[0;32mpass\033[0m\n");
        }
    } else if (testFailure) {
        fprintf(stderr, "\033[0;31mfail\033[0m\n");
    } else {
        fprintf(stderr, "\033[0;32mpass\033[0m\n");
    }

    return ++testsRun;
}


static void verifyConstructResult(result_t *constructResult, entry_t *entry) {
    if (entry->code.size) {
        // verify result code matches entry->code if present
        if (constructResult->returnData.size != entry->code.size || memcmp(constructResult->returnData.content, entry->code.content, entry->code.size)) {
            fputs("Code mismatch at address ", stderr);
            fprintAddress(stderr, (*entry->address));
            fprintf(stderr, ":\ninitcode result:\n");
            fprintData(stderr, constructResult->returnData);
            fprintf(stderr, "\nexpected:\n");
            fprintData(stderr, entry->code);
            fputc('\n', stderr);
            _exit(-1);
        }
    }
}

static void applyEntry(entry_t *entry) {
    if (entry->importPath) {
        loadConfig(entry->importPath, false);
        return;
    }
    if (entry->address == NULL) {
        entry->address = calloc(1, sizeof(address_t));
        entry->address->address[0] = 0xaa;
        entry->address->address[1] = 0xbb;
        static uint32_t anonymousId;
        *(uint32_t *)(&entry->address->address[15]) = anonymousId++;
    }
    if (entry->initCode.size) {
        address_t from;
        if (entry->creator) {
            AddressCopy(from, (*entry->creator));
        }
        // TODO support these parameters
        uint64_t gas = 0xffffffffffffffff;
        val_t value;
        value[0] = 0;
        value[1] = 0;
        value[2] = 0;

        result_t constructResult = evmConstruct(from, *entry->address, gas, value, entry->initCode);
        verifyConstructResult(&constructResult, entry);
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

static void jsonScanLogTopics(const char **iter, logChanges_t *log) {
    jsonScanChar(iter, '[');
    jsonScanWaste(iter);
    uint256_t topics[MAX_LOG_TOPICS];
    if (**iter != ']') do {
        const char *topic = jsonScanStr(iter);
        jsonSkipExpectedChar(&topic, '0');
        jsonSkipExpectedChar(&topic, 'x');
        clear256(topics+log->topicCount);
        while (*topic != '"') {
            shiftl256((topics+log->topicCount), 4, (topics+log->topicCount));
            LOWER(LOWER_P((topics+log->topicCount))) |= hexString8ToUint8(*topic);
            topic++;
        }
        log->topicCount++;
        jsonScanWaste(iter);
        if (**iter == ',') {
            jsonSkipExpectedChar(iter, ',');
            jsonScanWaste(iter);
            continue;
        } else {
            break;
        }
    } while (1);
    jsonSkipExpectedChar(iter, ']');
    log->topics = calloc(log->topicCount, sizeof(uint256_t));
    for (uint16_t i = log->topicCount; i--> 0;) {
        log->topics[log->topicCount - i - 1] = topics[i];
    }
}

static void jsonScanData(const char **iter, data_t *result){
    const char *data = jsonScanStr(iter);
    jsonSkipExpectedChar(&data, '0');
    jsonSkipExpectedChar(&data, 'x');
    result->size = (*iter - data - 1) / 2;
    result->content = malloc(result->size);
    for (size_t i = 0; i < result->size; i++) {
        result->content[i] = hexString16ToUint8(data);
        data += 2;
    }
}

static void jsonScanLog(const char **iter, logChanges_t **prev) {
    jsonScanChar(iter, '{');
    logChanges_t *log = calloc(1, sizeof(logChanges_t));
    log->prev = *prev;
    *prev = log;
    jsonScanWaste(iter);
    if (**iter != '}') do {
        const char *logHeading = jsonScanStr(iter);
        size_t logHeadingLen = *iter - logHeading - 1;
        jsonScanChar(iter, ':');
        if (logHeadingLen == 6 && *logHeading == 't') {
            // topics
            jsonScanLogTopics(iter, log);
        } else if (logHeadingLen == 4 && *logHeading == 'd') {
            // data
            jsonScanData(iter, &log->data);
        } else {
            fprintf(stderr, "Unexpected log heading: ");
            for (size_t i = 0; i < logHeadingLen; i++) {
                fputc(logHeading[i], stderr);
            }
            fputc('\n', stderr);
            _exit(1);
        }
        jsonScanWaste(iter);
        if (**iter == ',') {
            jsonSkipExpectedChar(iter, ',');
            jsonScanWaste(iter);
        } else {
            break;
        }
    } while (1);
    jsonSkipExpectedChar(iter, '}');
}

static void jsonScanAccountLogs(const char **iter, logsEntry_t **prev) {
    logsEntry_t *accountLogs = calloc(1, sizeof(logsEntry_t));
    accountLogs->prev = *prev;
    *prev = accountLogs;

    const char *addressStr = jsonScanStr(iter);
    size_t addressLen = *iter - addressStr - 1;
    if (addressLen != 42) {
        fprintf(stderr, "Unexpected address len %zu\n", addressLen);
    }
    accountLogs->address = AddressFromHex42(addressStr);
    jsonScanChar(iter, ':');
    jsonScanChar(iter, '[');

    jsonScanWaste(iter);
    if (**iter != ']') do {
        jsonScanLog(iter, &accountLogs->logs);

        jsonScanWaste(iter);
        if (**iter == ',') {
            jsonSkipExpectedChar(iter, ',');
            jsonScanWaste(iter);
            continue;
        } else {
            break;
        }
    } while (1);
    jsonSkipExpectedChar(iter, ']');
}

static address_t *jsonReadAddress(const char **iter) {
    jsonSkipExpectedChar(iter, '0');
    jsonSkipExpectedChar(iter, 'x');
    address_t *address = malloc(sizeof(address_t));
    for (unsigned int i = 0; i < 20; i++) {
        address->address[i] = hexString16ToUint8(*iter);
        *iter += 2;
    }
    return address;
}

static address_t *jsonReadAddressString(const char **iter) {
    jsonSkipExpectedChar(iter, '"');
    address_t *address = jsonReadAddress(iter);
    jsonSkipExpectedChar(iter, '"');
    return address;
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
                entry.address = jsonReadAddressString(iter);
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
            case 'aerc':
                // creator
                entry.creator = jsonReadAddressString(iter);
                break;
            case 'tini':
                // init, initcode, initCode
                {
                    const char *start = jsonScanStr(iter);
                    if (*(uint16_t *)start == 'x0') {
                        jsonSkipExpectedChar(&start, '0');
                        jsonSkipExpectedChar(&start, 'x');
                        entry.initCode.size = (*iter - start) / 2;
                        entry.initCode.content = calloc(entry.initCode.size, 1);
                        for (unsigned int i = 0; i < entry.initCode.size; i++) {
                            entry.initCode.content[i] = hexString16ToUint8(start);
                            start += 2;
                        }
                    } else {
                        size_t len = *iter - start - 1;
                        char *initcodePath = malloc(len + 1);
                        memcpy(initcodePath, start, len);
                        initcodePath[len] = '\0';
                        entry.initCode = assemblePath(initcodePath);
                        free(initcodePath);
                    }
                }
                break;
            case 'edoc':
                // code
                {
                    const char *start = jsonScanStr(iter);
                    if (*(uint16_t *)start == 'x0') {
                        jsonSkipExpectedChar(&start, '0');
                        jsonSkipExpectedChar(&start, 'x');
                        entry.code.size = (*iter - start) / 2;
                        entry.code.content = calloc(entry.code.size, 1);
                        for (unsigned int i = 0; i < entry.code.size; i++) {
                            entry.code.content[i] = hexString16ToUint8(start);
                            start += 2;
                        }
                    } else {
                        size_t len = *iter - start - 1;
                        entry.path = malloc(len + 1);
                        memcpy(entry.path, start, len);
                        entry.path[len] = '\0';
                        entry.code = assemblePath(entry.path);
                    }
                }
                break;
            case 'opmi':
                // import
                {
                    const char *start = jsonScanStr(iter);
                    size_t len = *iter - start - 1;
                    entry.importPath = malloc(len + 1);
                    memcpy(entry.importPath, start, len);
                    entry.importPath[len] = '\0';
                }
                break;
            case 'snoc':
                // construct
                {
                    const char *start = jsonScanStr(iter);
                    size_t len = *iter - start - 1;
                    entry.path = malloc(len + 1);
                    memcpy(entry.path, start, len);
                    entry.path[len] = '\0';
                    entry.initCode = defaultConstructorForPath(entry.path);
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
                        *(testResults.tail) = &test->result;
                        testResults.tail = &test->result.next;
                        test->prev = entry.tests;
                        LOWER(LOWER(test->status)) = 1;
                        entry.tests = test;

                        if (**iter != '}') do {
                            const char *testHeading = jsonScanStr(iter);
                            size_t testHeadingLen = *iter - testHeading - 1;
                            jsonScanChar(iter, ':');
                            if (testHeadingLen == 10 && *testHeading == 'a') {
                                // accessList
                                jsonScanChar(iter, '{');
                                jsonScanWaste(iter);
                                if (**iter != '}') do {
                                    accessList_t *accessList = calloc(1, sizeof(accessList_t));
                                    accessList->prev = test->accessList;
                                    test->accessList = accessList;
                                    const char *accessListAccount = jsonScanStr(iter);
                                    size_t accessListAccountLen = *iter - accessListAccount - 1;
                                    if (accessListAccountLen != 42) {
                                        fprintf(stderr, "Unexpected address length %zu\n", accessListAccountLen);
                                        _exit(1);
                                    }
                                    accessList->address = AddressFromHex42(accessListAccount);

                                    jsonScanChar(iter, ':');

                                    jsonScanChar(iter, '[');
                                    if (**iter != ']') do {
                                        const char *accessListSlot = jsonScanStr(iter);

                                        jsonSkipExpectedChar(&accessListSlot, '0');
                                        jsonSkipExpectedChar(&accessListSlot, 'x');


                                        accessListStorage_t *slot = calloc(1, sizeof(accessListStorage_t));
                                        slot->prev = accessList->storage;
                                        accessList->storage = slot;

                                        while (*accessListSlot != '"') {
                                            shiftl256(&slot->key, 4, &slot->key);
                                            LOWER(LOWER(slot->key)) |= hexString8ToUint8(*accessListSlot);
                                            accessListSlot++;
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
                                    jsonScanChar(iter, ']');
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
                            } else if (testHeadingLen == 4 && *testHeading == 'l') {
                                // logs
                                jsonScanChar(iter, '{');
                                jsonScanWaste(iter);
                                if (**iter != '}') do {
                                    jsonScanAccountLogs(iter, &test->logs);

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
                            } else {
                                const char *testValue = jsonScanStr(iter);
                                size_t testValueLength = *iter - testValue - 1;
                                if (testHeadingLen == 5 && *testHeading == 'i') {
                                    // input
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    test->input.size = (testValueLength - 2) / 2;
                                    test->input.content = malloc(test->input.size);
                                    uint32_t whitespaceCount = 0;
                                    for (size_t i = 0; i < test->input.size; i++) {
                                        char curr = testValue[i * 2 + whitespaceCount];
                                        if (jsonIgnores(curr)) {
                                            if (whitespaceCount++ & 1) {
                                                test->input.size--;
                                            }
                                            if (curr == '\n') {
                                                lineNumber++;
                                            }
                                            i--;
                                            continue;
                                        }
                                        test->input.content[i] = hexString16ToUint8(testValue + i * 2 + whitespaceCount);
                                    }
                                } else if (testHeadingLen == 4 && *testHeading == 'n') {
                                    // name
                                    test->name = malloc(testValueLength + 1);
                                    strncpy(test->name, testValue, testValueLength);
                                    test->name[testValueLength] = '\0';
                                } else if (testHeadingLen == 5 && *testHeading == 'd') {
                                    // debug
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    testValueLength -= 2;
                                    for (size_t i = 0; i < testValueLength; i++) {
                                        test->debug <<= 4;
                                        test->debug |= hexString8ToUint8(testValue[i]);
                                    }
                                } else if (testHeadingLen == 11 && *testHeading == 'b') {
                                    // blockNumber
                                    test->blockNumber = malloc(8);
                                    *test->blockNumber = 0;
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    testValueLength -= 2;
                                    for (size_t i = 0; i < testValueLength; i++) {
                                        *test->blockNumber <<= 4;
                                        *test->blockNumber |= hexString8ToUint8(testValue[i]);
                                    }
                                } else if (testHeadingLen == 9 && *testHeading == 't') {
                                    // timestamp
                                    test->timestamp = malloc(8);
                                    *test->timestamp = 0;
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    testValueLength -= 2;
                                    for (size_t i = 0; i < testValueLength; i++) {
                                        *test->timestamp <<= 4;
                                        *test->timestamp |= hexString8ToUint8(testValue[i]);
                                    }
                                } else if (testHeadingLen == 6 && *testHeading == 'o') {
                                    // output
                                    test->outputSpecified = true;
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    test->output.size = (testValueLength - 2) / 2;
                                    test->output.content = malloc(test->output.size);
                                    for (size_t i = 0; i < test->output.size; i++) {
                                        test->output.content[i] = hexString16ToUint8(testValue + i * 2);
                                    }
                                } else if (testHeadingLen == 5 && *testHeading == 'v') {
                                    // value
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    while (*testValue != '"') {
                                        test->value[0] <<= 4;
                                        test->value[0] |= test->value[1] >> 28;
                                        test->value[1] <<= 4;
                                        test->value[1] |= test->value[2] >> 28;
                                        test->value[2] <<= 4;
                                        test->value[2] |= hexString8ToUint8(*testValue);
                                        testValue++;
                                    }
                                } else if (testHeadingLen == 4 && *testHeading == 'f') {
                                    // from
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    for (unsigned int i = 0; i < 20; i++) {
                                        test->from.address[i] = hexString16ToUint8(testValue);
                                        testValue += 2;
                                    }
                                } else if (testHeadingLen == 2 && *testHeading == 't') {
                                    // to
                                    if (test->to) {
                                        free(test->to);
                                    }
                                    test->to = jsonReadAddress(&testValue);
                                } else if (testHeadingLen == 7 && *testHeading == 'g') {
                                    // gasUsed
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    test->result.gasUsedBegin = testValue;
                                    testValueLength -= 2;
                                    test->result.gasUsedEnd = testValue + testValueLength;
                                    for (size_t i = 0; i < testValueLength; i++) {
                                        test->gasUsed <<= 4;
                                        test->gasUsed |= hexString8ToUint8(testValue[i]);
                                    }
                                } else if (testHeadingLen == 3 && *testHeading == 'g') {
                                    // gas
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    while (*testValue != '"') {
                                        test->gas <<= 4;
                                        test->gas |= hexString8ToUint8(*testValue);
                                        testValue++;
                                    }
                                } else if (testHeadingLen == 2 && *testHeading == 'o') {
                                    // op
                                    const char *end;
                                    test->op = parseOp(testValue, &end);
                                } else if (testHeadingLen == 6 && *testHeading == 's') {
                                    // status
                                    jsonSkipExpectedChar(&testValue, '0');
                                    jsonSkipExpectedChar(&testValue, 'x');
                                    clear256(&test->status);
                                    while (*testValue != '"') {
                                        shiftl256(&test->status, 4, &test->status);
                                        LOWER(LOWER(test->status)) |= hexString8ToUint8(*testValue);
                                        testValue++;
                                    }
                                } else {
                                    fputs("Unexpected test heading: ", stderr);
                                    for (size_t i = 0; i < testHeadingLen; i++) {
                                        fputc(testHeading[i], stderr);
                                    }
                                    fputc('\n', stderr);
                                }
                            }
                            if (test->result.gasUsedEnd == NULL) {
                                test->result.gasUsedEnd = *iter;
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

                        if (test->result.gasUsedBegin == NULL) {
                            test->result.gasUsedBegin = test->result.gasUsedEnd;
                        }

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
                {
                    const char *end = heading - 1;
                    jsonScanStr(&end);
                    size_t headingLen = end - heading - 1;
                    char *headingCopy = malloc(sizeof(headingLen + 1));
                    memcpy(headingCopy, heading, headingLen);
                    headingCopy[headingLen] = '\0';
                    fprintf(stderr, "Unexpected entry heading \"%s\" on line %" PRIu64 "\n", headingCopy, lineNumber);
                }
                jsonSkipEntryValue(iter);
                break;
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
    testResults.head = NULL;
    testResults.tail = &testResults.head;
    lineNumber = 1;

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
    if (anyTestFailure) {
        _exit(1);
    }
}

typedef char char_t;
VECTOR(char, file);

static void updateConfig(const char *configContents, const char *dstPath) {
    file_t file;
    file_init(&file, 500000);
    const char *start = configContents;
    const testResult_t *testResult = testResults.head;
    while (testResult) {
        const char *end = testResult->gasUsedBegin;
        file_ensure(&file, file.num_chars + (end - start));
        memcpy(file.chars + file.num_chars, start, end - start);
        file.num_chars += (end - start);
        start = testResult->gasUsedEnd;
        if (start == end) {
            // need to add gasUsed key
            const char gasUsedHeader[] = ",\n                \"gasUsed\": \"0x";
            file_ensure(&file, file.num_chars + (end - start));
            memcpy(file.chars + file.num_chars, gasUsedHeader, sizeof(gasUsedHeader) - 1);
            file.num_chars += sizeof(gasUsedHeader) - 1;
        }
        file_ensure(&file, file.num_chars + 18);
        file.num_chars += snprintf(file.chars + file.num_chars, file.buffer_size - file.num_chars, "%" PRIx64, testResult->gasUsed);
        if (start == end) {
            file.chars[file.num_chars] = '"';
            file.num_chars += 1;
        }
        testResult = testResult->next;
    }

    size_t remaining = strlen(start);
    memcpy(file.chars + file.num_chars, start, remaining);
    file.num_chars += remaining;

    int fd = open(dstPath, O_WRONLY);
    if (fd == -1) {
        perror(dstPath);
        exit(1);
    }
    ssize_t written = write(fd, file.chars, file.num_chars);
    if (written == -1) {
        perror(dstPath);
        exit(1);
    }
    close(fd);
    file_destroy(&file);
}

void loadConfig(const char *_configFile, int updateConfigFile) {
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
        {
            uint64_t prevLineNumber = lineNumber;
            struct testResults results = testResults;

            applyConfig(configContents);
            if (updateConfigFile) {
                updateConfig(configContents, _configFile);
            }

            lineNumber = prevLineNumber;
            testResults = results;
        }
        munmap(configContents, fstatus.st_size);
        close(fd);
    }
}
