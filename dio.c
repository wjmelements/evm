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

typedef struct storage_kv {
    char *key;
    char *value;
    struct storage_kv *next;
} storage_kv_t;

typedef struct account {
    char  address[ADDR_LEN];
    char  balance[HEX256_LEN];
    char  nonce[NONCE_LEN];
    char *code;               /* dynamically allocated "0x..." */
    storage_kv_t  *storage;  /* fetched key→value pairs */
    struct account *next;
} account_t;

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
    while (resp.len > 0 && (resp.buf[resp.len-1] == '\n' || resp.buf[resp.len-1] == '\r'))
        resp.buf[--resp.len] = '\0';
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

    account_t *accounts = NULL;
    char *output = runViaEvm(self, to, from, input, block, post, ctx, &accounts);
    writeConfig(accounts, to, from, input, block, outfile, output);
    free(output);
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
