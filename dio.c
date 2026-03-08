/*
 * dio - Generate evm test config files from on-chain state
 *
 * Usage:
 *   dio [provider-url] [outfile] [-o json] [file...]
 *
 * The call JSON (from -o, a file argument, or stdin) is the eth_call object:
 *   {"to": "0x...", "from": "0x...", "data": "0x...", "block": "latest"}
 *
 * Only "to" is required.  "block" defaults to "latest".
 * The generated config is written to outfile, or stdout if omitted.
 *
 * Options:
 *   -o <json>    Call JSON inline
 *
 * The provider URL may also be set via the ETH_RPC_URL environment variable.
 */

#include "ws.h"
#include <curl/curl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* =========================================================
 * Data structures
 * ========================================================= */

#define ADDR_LEN    43      /* "0x" + 40 hex + NUL */
#define HEX256_LEN  68      /* "0x" + 64 hex + NUL */
#define NONCE_LEN   22      /* "0x" + up to 18 hex + NUL */
#define LINE_CAP    131072  /* matches evm's rpcBuf */
#define MAX_BATCH   8192    /* max requests in one JSON-RPC batch */

typedef struct storage_kv {
    char *key;
    char *value;
    struct storage_kv *next;
} storage_kv_t;

/* Storage key recorded during trace (before value is fetched) */
typedef struct storage_key {
    char key[HEX256_LEN];
    struct storage_key *next;
} storage_key_t;

typedef struct account {
    char  address[ADDR_LEN];
    char  balance[HEX256_LEN];
    char  nonce[NONCE_LEN];
    char *code;               /* dynamically allocated "0x..." */
    storage_kv_t  *storage;  /* fetched key→value pairs */
    storage_key_t *keys;     /* keys to fetch (from access list) */
    struct account *next;
} account_t;

/* Metadata for one request in a batch, used to route responses */
typedef struct {
    char addr[ADDR_LEN];
    char key[HEX256_LEN];   /* empty if not a storage slot */
    int  field;             /* 0=balance 1=nonce 2=code 3=storage */
} batch_meta_t;

/* =========================================================
 * Growing string buffer
 * ========================================================= */

typedef struct { char *buf; size_t len, cap; } strbuf_t;

static void sbAppend(strbuf_t *sb, const char *s, size_t n) {
    if (sb->len + n + 1 > sb->cap) {
        size_t nc = sb->cap ? sb->cap * 2 : 4096;
        while (nc < sb->len + n + 1) nc *= 2;
        sb->buf = realloc(sb->buf, nc);
        sb->cap = nc;
    }
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}

static void sbFmt(strbuf_t *sb, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (sb->len + (size_t)n + 1 > sb->cap) {
        size_t nc = sb->cap ? sb->cap * 2 : 4096;
        while (nc < sb->len + (size_t)n + 1) nc *= 2;
        sb->buf = realloc(sb->buf, nc);
        sb->cap = nc;
    }
    vsnprintf(sb->buf + sb->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    sb->len += (size_t)n;
}

/* =========================================================
 * Minimal JSON helpers
 *
 * These cover only the JSON shapes produced/consumed by
 * evm -n and well-formed JSON-RPC responses.
 * ========================================================= */

static void skipWs(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n')
        (*p)++;
}

/*
 * Skip a JSON value starting at *p, advancing *p past it.
 * Handles strings, objects, arrays, numbers/literals.
 */
static void jSkip(const char **p) {
    skipWs(p);
    if (**p == '"') {
        (*p)++;
        while (**p && **p != '"') {
            if (**p == '\\' && (*p)[1]) (*p)++;
            (*p)++;
        }
        if (**p == '"') (*p)++;
        return;
    }
    if (**p == '{' || **p == '[') {
        char open = **p, close = (open == '{') ? '}' : ']';
        int depth = 1;
        (*p)++;
        while (**p && depth > 0) {
            if (**p == '"') {
                (*p)++;
                while (**p && **p != '"') {
                    if (**p == '\\' && (*p)[1]) (*p)++;
                    (*p)++;
                }
                if (**p == '"') (*p)++;
            } else if (**p == open) {
                depth++; (*p)++;
            } else if (**p == close) {
                depth--; (*p)++;
            } else {
                (*p)++;
            }
        }
        return;
    }
    /* number, true, false, null */
    while (**p && **p != ',' && **p != '}' && **p != ']' &&
           **p != ' ' && **p != '\t' && **p != '\r' && **p != '\n')
        (*p)++;
}

/*
 * Scan forward in p for "key": and return a pointer to the value.
 * Sufficient for flat JSON-RPC messages.  Returns NULL if not found.
 */
static const char *jFind(const char *p, const char *key) {
    size_t klen = strlen(key);
    while (*p) {
        if (*p++ == '"' && strncmp(p, key, klen) == 0 && p[klen] == '"') {
            p += klen + 1;
            while (*p == ' ' || *p == '\t') p++;
            if (*p++ != ':') continue;
            while (*p == ' ' || *p == '\t') p++;
            return p;
        }
    }
    return NULL;
}

/*
 * Copy the quoted JSON string at p into buf (NUL-terminated, max buflen).
 * Returns char count, or -1 if p is not a quoted string.
 */
static int jStr(const char *p, char *buf, size_t buflen) {
    if (!p || *p != '"') return -1;
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) p++;
        if (i + 1 < buflen) buf[i++] = *p;
        if (*p) p++;
    }
    buf[i] = '\0';
    return (int)i;
}

/*
 * Parse a JSON number or quoted "0x..." hex at p into uint64_t.
 */
static uint64_t jUint(const char *p) {
    if (!p) return 0;
    if (*p == '"') p++;
    if (p[0] == '0' && p[1] == 'x') {
        p += 2;
        uint64_t v = 0;
        for (unsigned c; (c = (unsigned char)*p); p++) {
            if      (c >= '0' && c <= '9') v = (v << 4) | (c - '0');
            else if (c >= 'a' && c <= 'f') v = (v << 4) | (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v = (v << 4) | (c - 'A' + 10);
            else break;
        }
        return v;
    }
    uint64_t v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    return v;
}

/*
 * Return a pointer to element n (0-based) in the JSON array at '['.
 * Returns NULL if out of range.
 */
static const char *jArrayGet(const char *p, int n) {
    if (!p || *p != '[') return NULL;
    p++;
    while (*p) {
        skipWs(&p);
        if (*p == ']') return NULL;
        if (n == 0) return p;
        jSkip(&p);
        n--;
        skipWs(&p);
        if (*p == ',') p++;
    }
    return NULL;
}

/*
 * Store pointers to the last n elements (n ≤ 2) of a JSON array into
 * out[0..n-1], where out[n-1] is the last element (EVM stack top).
 * Returns the actual count stored (may be < n for short arrays).
 */
static int jArrayTailN(const char *arr, int n, const char **out) {
    if (!arr || *arr != '[' || n <= 0 || n > 2) return 0;
    const char *buf[2];
    int count = 0;
    const char *p = arr + 1;
    while (*p) {
        skipWs(&p);
        if (*p == ']') break;
        buf[count % n] = p;
        count++;
        jSkip(&p);
        skipWs(&p);
        if (*p == ',') p++;
    }
    int found = count < n ? count : n;
    for (int i = 0; i < found; i++)
        out[i] = buf[(count - found + i) % n];
    return found;
}

/*
 * Return a dynamically-allocated copy of the quoted JSON string at p.
 * Returns "0x" if p is NULL or not a string.  Caller must free().
 */
static char *jStrDup(const char *p) {
    if (!p || *p != '"') return strdup("0x");
    p++;
    const char *end = p;
    while (*end && *end != '"') end++;
    size_t len = end - p;
    char *s = malloc(len + 1);
    memcpy(s, p, len);
    s[len] = '\0';
    return s;
}

/*
 * Extract "key" string from json, prepend "0x" if absent.
 * Returns a dynamically-allocated string.  Caller must free().
 */
static char *jStrHex(const char *json, const char *key) {
    const char *p = jFind(json, key);
    if (!p || *p != '"') return strdup("0x");
    p++;
    const char *end = p;
    while (*end && *end != '"') end++;
    size_t len = end - p;
    int has0x = (len >= 2 && p[0] == '0' && p[1] == 'x');
    char *s = malloc(len + (has0x ? 1 : 3));
    if (has0x) {
        memcpy(s, p, len); s[len] = '\0';
    } else {
        s[0] = '0'; s[1] = 'x';
        memcpy(s + 2, p, len); s[len + 2] = '\0';
    }
    return s;
}

/*
 * In a JSON-RPC batch response array, find the element with "id": targetId
 * and return a newly-allocated copy of its "result" string value.
 * Caller must free(). Returns NULL if not found.
 */
static char *resultById(const char *resp, uint64_t targetId) {
    const char *p = resp;
    if (*p == '[') p++;
    while (*p) {
        skipWs(&p);
        if (*p == ']' || !*p) break;
        const char *elem = p;
        if (jUint(jFind(elem, "id")) == targetId) {
            const char *rv = jFind(elem, "result");
            if (rv && *rv == '"') {
                rv++;
                const char *end = rv;
                while (*end && *end != '"') end++;
                size_t len = end - rv;
                char *s = malloc(len + 1);
                memcpy(s, rv, len);
                s[len] = '\0';
                return s;
            }
        }
        jSkip(&p);
        skipWs(&p);
        if (*p == ',') p++;
    }
    return NULL;
}

/* =========================================================
 * Account management
 * ========================================================= */

static void normalizeAddr(const char *in, char *out) {
    const char *s = (in[0] == '0' && in[1] == 'x') ? in + 2 : in;
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 40; i++) {
        unsigned char c = (unsigned char)s[i];
        out[2 + i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    out[42] = '\0';
}

static account_t *ensureAccount(account_t **head, const char *addr) {
    char norm[ADDR_LEN];
    normalizeAddr(addr, norm);
    for (account_t *a = *head; a; a = a->next)
        if (strcmp(a->address, norm) == 0) return a;
    account_t *a = calloc(1, sizeof(account_t));
    memcpy(a->address, norm, ADDR_LEN);
    strcpy(a->balance, "0x0");
    strcpy(a->nonce,   "0x0");
    a->code = strdup("0x");
    a->next = *head;
    *head = a;
    return a;
}

/*
 * Normalize a storage key: strip leading zeros, keep at least one digit.
 * "0x000...abc" -> "0xabc",  "0x000...0" -> "0x0"
 */
static void normalizeKey(const char *in, char *out, size_t outlen) {
    const char *s = (in[0] == '0' && in[1] == 'x') ? in + 2 : in;
    while (*s == '0' && *(s + 1)) s++;
    snprintf(out, outlen, "0x%s", s);
}

static void addStorage(account_t *acct, const char *key, const char *val) {
    const char *v = (val[0] == '0' && val[1] == 'x') ? val + 2 : val;
    int allZero = 1;
    for (const char *c = v; *c; c++) if (*c != '0') { allZero = 0; break; }
    if (allZero) return;

    char normKey[HEX256_LEN];
    normalizeKey(key, normKey, sizeof(normKey));
    for (storage_kv_t *s = acct->storage; s; s = s->next)
        if (strcmp(s->key, normKey) == 0) return;

    storage_kv_t *s = calloc(1, sizeof(storage_kv_t));
    s->key   = strdup(normKey);
    s->value = strdup(val);
    s->next  = acct->storage;
    acct->storage = s;
}

/* Record a storage key to be fetched for this account (deduped, normalized). */
static void addAccessKey(account_t *acct, const char *key) {
    char norm[HEX256_LEN];
    normalizeKey(key, norm, sizeof(norm));
    for (storage_key_t *k = acct->keys; k; k = k->next)
        if (strcmp(k->key, norm) == 0) return;
    storage_key_t *k = calloc(1, sizeof(storage_key_t));
    strncpy(k->key, norm, sizeof(k->key) - 1);
    k->next  = acct->keys;
    acct->keys = k;
}

/* =========================================================
 * EVM stack helpers
 * ========================================================= */

/*
 * Convert a 32-byte stack value ("0x000...address") to a
 * 0x-prefixed lowercase 20-byte address string.
 */
static void stackToAddress(const char *val, char *out) {
    const char *s = (val[0] == '0' && val[1] == 'x') ? val + 2 : val;
    size_t len = strlen(s);
    if (len > 40) s += (len - 40);
    int pad = (int)(40 - (len < 40 ? len : 40));
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < pad; i++) out[2 + i] = '0';
    for (int i = pad; i < 40; i++) {
        unsigned char c = (unsigned char)s[i - pad];
        out[2 + i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    out[42] = '\0';
}

/* =========================================================
 * Find evm binary
 * ========================================================= */

static char evmBinPath[4096];

/*
 * Look for an "evm" sibling next to the running binary (argv[0]).
 * Falls back to "evm" for PATH resolution if not found.
 */
static const char *findEvm(const char *self) {
    const char *slash = strrchr(self, '/');
    if (slash) {
        snprintf(evmBinPath, sizeof(evmBinPath),
                 "%.*s/evm", (int)(slash - self), self);
        struct stat st;
        if (stat(evmBinPath, &st) == 0 && (st.st_mode & S_IXUSR))
            return evmBinPath;
    }
    return "evm";
}

/* =========================================================
 * JSON-RPC post function type
 *
 * Sends a NUL-terminated JSON payload and returns a
 * dynamically-allocated NUL-terminated response string.
 * Caller must free().  Returns NULL on error.
 * ========================================================= */
typedef char *(*postFn)(const char *payload, void *ctx);

/* =========================================================
 * HTTP and WebSocket post implementations
 * ========================================================= */

typedef struct { const char *url; CURL *curl; } http_ctx_t;

static size_t curlWrite(char *data, size_t sz, size_t n, void *userp) {
    sbAppend((strbuf_t *)userp, data, sz * n);
    return sz * n;
}

static char *httpPost(const char *payload, void *ctx) {
    http_ctx_t *hctx = ctx;
    strbuf_t resp = {0};

    struct curl_slist *hdrs = NULL;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

    curl_easy_setopt(hctx->curl, CURLOPT_URL,           hctx->url);
    curl_easy_setopt(hctx->curl, CURLOPT_POST,          1L);
    curl_easy_setopt(hctx->curl, CURLOPT_POSTFIELDS,    payload);
    curl_easy_setopt(hctx->curl, CURLOPT_POSTFIELDSIZE, (long)strlen(payload));
    curl_easy_setopt(hctx->curl, CURLOPT_HTTPHEADER,    hdrs);
    curl_easy_setopt(hctx->curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(hctx->curl, CURLOPT_WRITEDATA,     &resp);

    CURLcode rc = curl_easy_perform(hctx->curl);
    curl_slist_free_all(hdrs);

    if (rc != CURLE_OK) {
        fprintf(stderr, "dio: HTTP: %s\n", curl_easy_strerror(rc));
        free(resp.buf);
        return NULL;
    }
    return resp.buf ? resp.buf : strdup("");
}


/* =========================================================
 * Subprocess proxy (evm -x -n fallback)
 * ========================================================= */

/*
 * Spawn `evm -x -n -o <callJson>` with bidirectional pipes.
 *   *toChild   — parent writes here → child stdin
 *   *fromChild — parent reads here  ← child stdout
 * Returns the child PID.
 */
static pid_t spawnEvm(const char *evm, const char *callJson,
                      FILE **toChild, FILE **fromChild) {
    int toFds[2], fromFds[2];
    if (pipe(toFds) == -1 || pipe(fromFds) == -1) {
        perror("dio: pipe");
        _exit(1);
    }
    pid_t pid = fork();
    if (pid == -1) { perror("dio: fork"); _exit(1); }
    if (pid == 0) {
        dup2(toFds[0],   STDIN_FILENO);
        dup2(fromFds[1], STDOUT_FILENO);
        close(toFds[0]);  close(toFds[1]);
        close(fromFds[0]); close(fromFds[1]);
        char *args[] = { (char *)evm, "-x", "-n", "-o", (char *)callJson, NULL };
        execvp(evm, args);
        perror(evm);
        _exit(1);
    }
    close(toFds[0]);
    close(fromFds[1]);
    *toChild   = fdopen(toFds[1],   "w");
    *fromChild = fdopen(fromFds[0], "r");
    return pid;
}

/*
 * Write a sorted batch response [code, nonce, balance] to f.
 *
 * network.c's nextResultHex() scans for "result" values in forward
 * order and assigns them: first=code, second=nonce, third=balance.
 * We must therefore write the three elements sorted ascending by id.
 */
static void writeSortedBatch(FILE *f,
                              uint64_t codeId,    const char *code,
                              uint64_t nonceId,   const char *nonce,
                              uint64_t balanceId, const char *balance) {
    uint64_t    ids[3]  = { codeId,  nonceId,  balanceId };
    const char *vals[3] = { code,    nonce,    balance   };
    for (int i = 1; i < 3; i++) {
        uint64_t ki = ids[i]; const char *vi = vals[i];
        int j = i - 1;
        while (j >= 0 && ids[j] > ki) {
            ids[j + 1] = ids[j]; vals[j + 1] = vals[j]; j--;
        }
        ids[j + 1] = ki; vals[j + 1] = vi;
    }
    fputc('[', f);
    for (int i = 0; i < 3; i++) {
        if (i) fputc(',', f);
        fprintf(f, "{\"jsonrpc\":\"2.0\",\"id\":%" PRIu64 ",\"result\":\"%s\"}",
                ids[i], vals[i] ? vals[i] : "0x");
    }
    fputs("]\n", f);
    fflush(f);
}

/*
 * Run the call via `evm -x -n`, proxying its JSON-RPC requests through
 * the provided post function.  Collects account state into *accounts.
 * Returns a newly-allocated "0x..." output hex string.  Caller must free().
 */
static char *runViaEvm(
    const char *self,
    const char *to, const char *from, const char *data,
    const char *block,
    postFn post, void *ctx,
    account_t **accounts)
{
    char callJson[512];
    snprintf(callJson, sizeof(callJson),
             "{\"to\":\"%s\",\"from\":\"%s\",\"data\":\"%s\"}",
             to, from, data);

    FILE *toChild, *fromChild;
    pid_t pid = spawnEvm(findEvm(self), callJson, &toChild, &fromChild);

    char *line   = malloc(LINE_CAP);
    char *output = NULL;

    while (fgets(line, LINE_CAP, fromChild)) {
        char *nl = line + strlen(line);
        while (nl > line && (nl[-1] == '\n' || nl[-1] == '\r')) *--nl = '\0';
        if (!*line) continue;

        if (*line == '[') {
            /* ---- Account batch: [getCode, getTxCount, getBalance] ---- */
            const char *elem0   = jArrayGet(line, 0);
            const char *params0 = jFind(elem0, "params");
            char addr[ADDR_LEN];
            jStr(jArrayGet(params0, 0), addr, sizeof(addr));
            account_t *acct = ensureAccount(accounts, addr);

            uint64_t codeId    = jUint(jFind(jArrayGet(line, 0), "id"));
            uint64_t nonceId   = jUint(jFind(jArrayGet(line, 1), "id"));
            uint64_t balanceId = jUint(jFind(jArrayGet(line, 2), "id"));

            char *resp = post(line, ctx);
            if (!resp) {
                fputs("dio: RPC failed for account batch\n", stderr);
                _exit(1);
            }

            char *code    = resultById(resp, codeId);
            char *nonce   = resultById(resp, nonceId);
            char *balance = resultById(resp, balanceId);
            free(resp);

            free(acct->code);
            acct->code = code ? code : strdup("0x");
            if (nonce)   strncpy(acct->nonce,   nonce,   sizeof(acct->nonce)   - 1);
            if (balance) strncpy(acct->balance, balance, sizeof(acct->balance) - 1);
            free(nonce);
            free(balance);

            writeSortedBatch(toChild,
                             codeId,    acct->code,
                             nonceId,   acct->nonce,
                             balanceId, acct->balance);

        } else if (strstr(line, "\"eth_blockNumber\"")) {
            uint64_t id = jUint(jFind(line, "id"));
            fprintf(toChild,
                    "{\"jsonrpc\":\"2.0\",\"id\":%" PRIu64 ",\"result\":\"%s\"}\n",
                    id, block);
            fflush(toChild);

        } else if (strstr(line, "\"eth_getStorageAt\"")) {
            const char *params = jFind(line, "params");
            char addr[ADDR_LEN], rawKey[HEX256_LEN];
            jStr(jArrayGet(params, 0), addr,   sizeof(addr));
            jStr(jArrayGet(params, 1), rawKey, sizeof(rawKey));
            account_t *acct = ensureAccount(accounts, addr);

            char *resp = post(line, ctx);
            if (!resp) {
                fputs("dio: RPC failed for eth_getStorageAt\n", stderr);
                _exit(1);
            }
            char val[HEX256_LEN];
            jStr(jFind(resp, "result"), val, sizeof(val));
            addStorage(acct, rawKey, val);
            fprintf(toChild, "%s\n", resp);
            fflush(toChild);
            free(resp);

        } else {
            /* ---- Final output from evm ---- */
            const char *ov = jFind(line, "output");
            if (ov && *ov == '"') {
                ov++;
                const char *end = ov;
                while (*end && *end != '"') end++;
                size_t len = end - ov;
                output = malloc(len + 1);
                memcpy(output, ov, len);
                output[len] = '\0';
            }
            break;
        }
    }

    fclose(toChild);
    fclose(fromChild);
    int status;
    waitpid(pid, &status, 0);
    free(line);

    if (!output || WEXITSTATUS(status) != 0) {
        fputs("dio: evm execution failed\n", stderr);
        _exit(1);
    }

    ensureAccount(accounts, to);
    if (strcmp(from, "0x0000000000000000000000000000000000000000") != 0)
        ensureAccount(accounts, from);

    return output;
}

/* =========================================================
 * debug_traceCall path
 * ========================================================= */

/*
 * Walk the structLogs array and build account entries with their
 * accessed storage keys.  Tracks the call stack to attribute
 * SLOAD to the correct contract context.
 */
static void buildAccessList(
    const char *structLogs,
    const char *to,
    const char *from,
    account_t **accounts)
{
    static char callStack[1024][ADDR_LEN];
    static int  callDepth;
    callDepth = 0;

    if (to && *to) {
        ensureAccount(accounts, to);
        normalizeAddr(to, callStack[callDepth++]);
    }
    if (from && *from &&
        strcmp(from, "0x0000000000000000000000000000000000000000") != 0) {
        ensureAccount(accounts, from);
    }

    const char *p = structLogs;
    if (!p || *p != '[') return;
    p++;

    while (*p) {
        skipWs(&p);
        if (*p == ']' || !*p) break;

        const char *entry = p;
        char op[32] = "";
        jStr(jFind(entry, "op"), op, sizeof(op));
        const char *stackArr = jFind(entry, "stack");

        if (strcmp(op, "SLOAD") == 0 && callDepth > 0) {
            const char *top[1];
            if (jArrayTailN(stackArr, 1, top) >= 1) {
                char rawKey[HEX256_LEN];
                jStr(top[0], rawKey, sizeof(rawKey));
                account_t *acct = ensureAccount(accounts, callStack[callDepth - 1]);
                addAccessKey(acct, rawKey);
            }

        } else if (strcmp(op, "CALL") == 0 || strcmp(op, "STATICCALL") == 0) {
            /* stack: gas(top=-1), addr(-2) */
            const char *top2[2];
            if (jArrayTailN(stackArr, 2, top2) >= 2) {
                char raw[HEX256_LEN], addr[ADDR_LEN];
                jStr(top2[0], raw, sizeof(raw));   /* stack[-2] = addr */
                stackToAddress(raw, addr);
                ensureAccount(accounts, addr);
                if (callDepth < 1024)
                    normalizeAddr(addr, callStack[callDepth++]);
            }

        } else if (strcmp(op, "DELEGATECALL") == 0) {
            /* Executes code at stack[-2] in the CALLER's storage context */
            const char *top2[2];
            if (jArrayTailN(stackArr, 2, top2) >= 2) {
                char raw[HEX256_LEN], codeAddr[ADDR_LEN];
                jStr(top2[0], raw, sizeof(raw));   /* stack[-2] = code addr */
                stackToAddress(raw, codeAddr);
                ensureAccount(accounts, codeAddr);
                if (callDepth < 1024) {
                    const char *ctx = callDepth > 0 ? callStack[callDepth - 1] : codeAddr;
                    strncpy(callStack[callDepth++], ctx, ADDR_LEN);
                }
            }

        } else if (strcmp(op, "EXTCODESIZE") == 0 ||
                   strcmp(op, "EXTCODECOPY") == 0) {
            const char *top[1];
            if (jArrayTailN(stackArr, 1, top) >= 1) {
                char raw[HEX256_LEN], addr[ADDR_LEN];
                jStr(top[0], raw, sizeof(raw));
                stackToAddress(raw, addr);
                ensureAccount(accounts, addr);
            }

        } else if (strcmp(op, "STOP")   == 0 ||
                   strcmp(op, "RETURN") == 0 ||
                   strcmp(op, "REVERT") == 0) {
            if (callDepth > 0) callDepth--;
        }

        jSkip(&p);
        skipWs(&p);
        if (*p == ',') p++;
    }
}

/*
 * Write the dio JSON config to outfile (stdout if NULL or "-").
 */
static void writeConfig(
    account_t  *accounts,
    const char *to,
    const char *from,
    const char *input,
    const char *block,
    const char *outfile,
    const char *output)
{
    FILE *f;
    if (outfile && strcmp(outfile, "-") != 0) {
        f = fopen(outfile, "w");
        if (!f) { perror(outfile); _exit(1); }
    } else {
        f = stdout;
    }

    fputs("[\n", f);

    for (account_t *a = accounts; a; a = a->next) {
        fputs("    {\n", f);
        fprintf(f, "        \"address\": \"%s\"", a->address);
        if (strcmp(a->balance, "0x0") != 0 && strcmp(a->balance, "0x") != 0)
            fprintf(f, ",\n        \"balance\": \"%s\"", a->balance);
        if (strcmp(a->nonce, "0x0") != 0 && strcmp(a->nonce, "0x") != 0)
            fprintf(f, ",\n        \"nonce\": \"%s\"", a->nonce);
        if (strcmp(a->code, "0x") != 0 && strcmp(a->code, "") != 0)
            fprintf(f, ",\n        \"code\": \"%s\"", a->code);
        if (a->storage) {
            fputs(",\n        \"storage\": {\n", f);
            int first = 1;
            for (storage_kv_t *s = a->storage; s; s = s->next) {
                if (!first) fputs(",\n", f);
                fprintf(f, "            \"%s\": \"%s\"", s->key, s->value);
                first = 0;
            }
            fputs("\n        }", f);
        }
        fputs("\n    },\n", f);
    }

    fputs("    {\n        \"tests\": [\n            {\n", f);
    fprintf(f, "                \"to\": \"%s\"", to);
    if (strcmp(from, "0x0000000000000000000000000000000000000000") != 0)
        fprintf(f, ",\n                \"from\": \"%s\"", from);
    if (input && strcmp(input, "0x") != 0)
        fprintf(f, ",\n                \"input\": \"%s\"", input);
    fprintf(f, ",\n                \"blockNumber\": \"%s\"", block);
    fputs(",\n                \"debug\": \"0x20\"", f);
    if (output)
        fprintf(f, ",\n                \"output\": \"%s\"", output);
    fputs("\n            }\n        ]\n    }\n]\n", f);

    if (f != stdout) {
        fclose(f);
        fprintf(stderr, "Wrote %s\n", outfile);
    }
}

/*
 * Main entry point for a single call.  Tries debug_traceCall first,
 * falls back to runViaEvm on error or unsupported method.
 */
static void run(
    const char *callJson,
    const char *outfile,
    postFn post, void *ctx,
    const char *self)
{
    char to[ADDR_LEN]   = "0x0000000000000000000000000000000000000000";
    char from[ADDR_LEN] = "0x0000000000000000000000000000000000000000";
    char block[32]      = "latest";

    char tmp[ADDR_LEN + 2];
    if (jStr(jFind(callJson, "to"),   tmp, sizeof(tmp)) > 0) normalizeAddr(tmp, to);
    if (jStr(jFind(callJson, "from"), tmp, sizeof(tmp)) > 0) normalizeAddr(tmp, from);

    const char *blkVal = jFind(callJson, "block");
    if (blkVal) jStr(blkVal, block, sizeof(block));

    const char *dataField = jFind(callJson, "data");
    if (!dataField) dataField = jFind(callJson, "input");
    char *input = jStrDup(dataField);
    if (input[0] != '0' || input[1] != 'x') {
        size_t ilen = strlen(input);
        char *t = malloc(ilen + 3);
        t[0] = '0'; t[1] = 'x';
        memcpy(t + 2, input, ilen + 1);
        free(input);
        input = t;
    }

    /* Resolve block number */
    if (strcmp(block, "latest") == 0) {
        char *resp = post(
            "{\"jsonrpc\":\"2.0\",\"id\":1,"
            "\"method\":\"eth_blockNumber\",\"params\":[]}",
            ctx);
        if (!resp) { fputs("dio: eth_blockNumber failed\n", stderr); _exit(1); }
        jStr(jFind(resp, "result"), block, sizeof(block));
        free(resp);
    }

    /* Try debug_traceCall */
    strbuf_t callObj = {0};
    sbFmt(&callObj, "{\"to\":\"%s\",\"from\":\"%s\"", to, from);
    if (strcmp(input, "0x") != 0)
        sbFmt(&callObj, ",\"data\":\"%s\"", input);
    sbAppend(&callObj, "}", 1);

    strbuf_t traceReq = {0};
    sbFmt(&traceReq,
          "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"debug_traceCall\","
          "\"params\":[%s,\"%s\","
          "{\"disableStorage\":true,\"disableMemory\":true}]}",
          callObj.buf, block);
    free(callObj.buf);

    char *traceResp = post(traceReq.buf, ctx);
    free(traceReq.buf);

    if (!traceResp || jFind(traceResp, "error")) {
        free(traceResp);
        account_t *accounts = NULL;
        char *output = runViaEvm(self, to, from, input, block, post, ctx, &accounts);
        writeConfig(accounts, to, from, input, block, outfile, output);
        free(output);
        free(input);
        return;
    }

    /* Parse trace */
    const char *result  = jFind(traceResp, "result");
    const char *logsArr = jFind(result, "structLogs");
    char *returnValue   = jStrHex(result, "returnValue");

    /* Build access list */
    account_t *accounts = NULL;
    buildAccessList(logsArr, to, from, &accounts);

    /* Batch-fetch all account state */
    strbuf_t batch = {0};
    batch_meta_t *meta = malloc(MAX_BATCH * sizeof(batch_meta_t));
    int bid = 1, metaCount = 0;

    sbAppend(&batch, "[", 1);
    int first = 1;
    for (account_t *a = accounts; a; a = a->next) {
        if (!first) sbAppend(&batch, ",", 1); first = 0;
        sbFmt(&batch,
              "{\"jsonrpc\":\"2.0\",\"id\":%d,"
              "\"method\":\"eth_getBalance\",\"params\":[\"%s\",\"%s\"]}",
              bid, a->address, block);
        strncpy(meta[metaCount].addr, a->address, ADDR_LEN);
        meta[metaCount].key[0] = '\0';
        meta[metaCount].field  = 0;
        metaCount++; bid++;

        sbFmt(&batch,
              ",{\"jsonrpc\":\"2.0\",\"id\":%d,"
              "\"method\":\"eth_getTransactionCount\",\"params\":[\"%s\",\"%s\"]}",
              bid, a->address, block);
        strncpy(meta[metaCount].addr, a->address, ADDR_LEN);
        meta[metaCount].key[0] = '\0';
        meta[metaCount].field  = 1;
        metaCount++; bid++;

        sbFmt(&batch,
              ",{\"jsonrpc\":\"2.0\",\"id\":%d,"
              "\"method\":\"eth_getCode\",\"params\":[\"%s\",\"%s\"]}",
              bid, a->address, block);
        strncpy(meta[metaCount].addr, a->address, ADDR_LEN);
        meta[metaCount].key[0] = '\0';
        meta[metaCount].field  = 2;
        metaCount++; bid++;

        for (storage_key_t *k = a->keys; k; k = k->next) {
            sbFmt(&batch,
                  ",{\"jsonrpc\":\"2.0\",\"id\":%d,"
                  "\"method\":\"eth_getStorageAt\","
                  "\"params\":[\"%s\",\"%s\",\"%s\"]}",
                  bid, a->address, k->key, block);
            strncpy(meta[metaCount].addr, a->address, ADDR_LEN);
            strncpy(meta[metaCount].key,  k->key, HEX256_LEN);
            meta[metaCount].field = 3;
            metaCount++; bid++;
        }
    }
    sbAppend(&batch, "]", 1);

    char *batchResp = NULL;
    if (metaCount > 0) {
        batchResp = post(batch.buf, ctx);
        if (!batchResp) { fputs("dio: batch RPC failed\n", stderr); _exit(1); }
    }
    free(batch.buf);

    if (batchResp) {
        const char *p = batchResp;
        if (*p == '[') p++;
        while (*p) {
            skipWs(&p);
            if (*p == ']' || !*p) break;
            const char *elem = p;
            int id = (int)jUint(jFind(elem, "id"));
            if (id >= 1 && id < bid) {
                int idx = id - 1;
                account_t *a = ensureAccount(&accounts, meta[idx].addr);
                const char *rv = jFind(elem, "result");
                switch (meta[idx].field) {
                    case 0: jStr(rv, a->balance, sizeof(a->balance)); break;
                    case 1: jStr(rv, a->nonce,   sizeof(a->nonce));   break;
                    case 2: free(a->code); a->code = jStrDup(rv);     break;
                    case 3: {
                        char val[HEX256_LEN];
                        jStr(rv, val, sizeof(val));
                        addStorage(a, meta[idx].key, val);
                        break;
                    }
                }
            }
            jSkip(&p);
            skipWs(&p);
            if (*p == ',') p++;
        }
        free(batchResp);
    }

    free(meta);
    free(traceResp);

    writeConfig(accounts, to, from, input, block, outfile, returnValue);
    free(returnValue);
    free(input);
}

/* =========================================================
 * main
 * ========================================================= */

static char *readAll(FILE *f) {
    strbuf_t sb = {0};
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        sbAppend(&sb, buf, n);
    return sb.buf ? sb.buf : strdup("");
}

static const char usage[] =
    "dio - Generate evm test config files from on-chain state\n"
    "\n"
    "Usage:\n"
    "    dio [provider-url] [outfile] [-o json] [file...]\n"
    "\n"
    "The call JSON (from -o, a file argument, or stdin) is the eth_call object:\n"
    "    {\"to\": \"0x...\", \"from\": \"0x...\", \"data\": \"0x...\", \"block\": \"latest\"}\n"
    "\n"
    "Only \"to\" is required.  \"block\" defaults to \"latest\".\n"
    "The generated config is written to outfile, or stdout if omitted.\n"
    "\n"
    "Options:\n"
    "    -o <json>    Call JSON inline\n"
    "\n"
    "The provider URL may also be set via the ETH_RPC_URL environment variable.\n";

int main(int argc, char *const argv[]) {
    static struct option longopts[] = {
        {"help", no_argument, NULL, 'h'},
        {NULL,   0,           NULL,  0 },
    };

    const char *inlineJson = NULL;
    int opt;
    while ((opt = getopt_long(argc, (char *const *)argv, "ho:", longopts, NULL)) != -1) {
        switch (opt) {
            case 'h': fputs(usage, stdout); return 0;
            case 'o': inlineJson = optarg;  break;
            default:  fputs(usage, stderr); return 1;
        }
    }

    const char *url     = (optind < argc) ? argv[optind++] : NULL;
    const char *outfile = (optind < argc) ? argv[optind++] : NULL;

    if (!url) url = getenv("ETH_RPC_URL");
    if (!url) { fputs(usage, stderr); return 1; }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    int ws = strncmp(url, "ws://",  5) == 0 ||
             strncmp(url, "wss://", 6) == 0;
    postFn post;
    void  *ctx;

    if (ws) {
        CURL *curl = wsConnect(url);
        if (!curl) { curl_global_cleanup(); return 1; }
        post = wsPost;
        ctx  = curl;
    } else {
        http_ctx_t *hctx = malloc(sizeof(http_ctx_t));
        hctx->url  = url;
        hctx->curl = curl_easy_init();
        post = httpPost;
        ctx  = hctx;
    }

    if (inlineJson) {
        run(inlineJson, outfile, post, ctx, argv[0]);
    } else if (optind < argc) {
        for (; optind < argc; optind++) {
            FILE *f = fopen(argv[optind], "r");
            if (!f) { perror(argv[optind]); return 1; }
            char *json = readAll(f);
            fclose(f);
            run(json, outfile, post, ctx, argv[0]);
            free(json);
        }
    } else {
        char *json = readAll(stdin);
        run(json, outfile, post, ctx, argv[0]);
        free(json);
    }

    if (ws) {
        curl_easy_cleanup(ctx);
    } else {
        http_ctx_t *hctx = ctx;
        curl_easy_cleanup(hctx->curl);
        free(hctx);
    }
    curl_global_cleanup();
    return 0;
}
