#include "dio.h"

#include <assert.h>
#include <strings.h>

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
            fprintf(stderr, "Config: when seeking '%c' found unexpected character '%c'", expected, ch);
            assert(expected == ch);
        }
    }
    (*iter)++;
}

static void jsonSkipExpectedChar(const char **iter, char expected) {
    if (**iter == expected) {
        (*iter)++;
    } else {
        fprintf(stderr, "Config: expecting '%c', found '%c'", expected, **iter);
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

typedef struct entry {
    address_t *address;
    val_t balance;
    uint64_t nonce;
    data_t code;
    storageEntry_t *storage;
} entry_t;


static void applyEntry(entry_t *entry) {
    if (entry->address == NULL) {
        entry->address = calloc(1, sizeof(address_t));
        entry->address->address[0] = 0xaa;
        entry->address->address[1] = 0xbb;
        static uint32_t anonymousId;
        *(uint32_t *)(&entry->address->address[15]) = anonymousId++;
    }
    evmMockCode(*entry->address, entry->code);
    evmMockNonce(*entry->address, entry->nonce);
    evmMockBalance(*entry->address, entry->balance);
    while (entry->storage != NULL) {
        storageEntry_t *prev = entry->storage;
        evmMockStorage(*entry->address, &prev->key, &prev->value);
        entry->storage = prev->prev;
        free(prev);
    }
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
                fprintf(stderr, "Unexpected entry heading: %04x\n", *(uint32_t *)heading);
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
}
