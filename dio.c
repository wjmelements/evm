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

#include "json.h"
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

typedef struct call_result {
    char  to[ADDR_LEN];
    char  from[ADDR_LEN];
    char  block[32];
    char  value[HEX256_LEN];
    char *input;
    char *output;
    char *logs;
    char *status;
    char *gasUsed;
    struct call_result *next;
} call_result_t;

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

static char        evmBinPath[4096];
static const char *evmPath;

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
typedef char *(*postFn)(const char *payload, size_t len, void *ctx);

/* =========================================================
 * HTTP and WebSocket post implementations
 * ========================================================= */

typedef struct { const char *url; CURL *curl; } http_ctx_t;

static size_t curlWrite(char *data, size_t sz, size_t n, void *userp) {
    sbAppend((strbuf_t *)userp, data, sz * n);
    return sz * n;
}

static char *httpPost(const char *payload, size_t len, void *ctx) {
    http_ctx_t *hctx = ctx;
    strbuf_t resp = {0};

    struct curl_slist *hdrs = NULL;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

    curl_easy_setopt(hctx->curl, CURLOPT_URL,           hctx->url);
    curl_easy_setopt(hctx->curl, CURLOPT_POST,          1L);
    curl_easy_setopt(hctx->curl, CURLOPT_POSTFIELDS,    payload);
    curl_easy_setopt(hctx->curl, CURLOPT_POSTFIELDSIZE, (long)len);
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
        char *args[] = { (char *)evm, "-glnsxo", (char *)callJson, NULL };
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

static void printMethod(const char *obj) {
    char method[64];
    if (jStr(jFind(obj, "method"), method, sizeof(method)) > 0) {
        fputc(' ', stderr);
        fputs(method, stderr);
    }
}

/* Print "dio: RPC failed: <method(s)>\n" and exit for a request that got NULL */
static void rpcFailed(const char *req) {
    fputs("dio: RPC failed:", stderr);
    if (*req == '[') {
        for (const char *elem = jArrayGet(req, 0); elem; elem = jArrayNext(elem))
            printMethod(elem);
    } else {
        printMethod(req);
    }
    fputc('\n', stderr);
    _exit(1);
}

/* Print "dio: <method> error: <obj>\n" and exit */
static void rpcError(const char *req, const char *errField) {
    fputs("dio:", stderr);
    printMethod(req);
    fputs(" error: ", stderr);
    char *errStr = jValDup(errField);
    fputs(errStr ? errStr : "?", stderr);
    free(errStr);
    fputc('\n', stderr);
    _exit(1);
}

/* Check a batch resp for error objects; match method names from the batch req */
static void checkBatchErrors(const char *resp, const char *req) {
    const char *qHead = jArrayGet(req, 0);
    for (const char *rElem = jArrayGet(resp, 0); rElem; rElem = jArrayNext(rElem)) {
        const char *errField = jFind(rElem, "error");
        if (!errField) continue;
        uint64_t id = jUint(jFind(rElem, "id"));
        for (const char *qElem = qHead; qElem; qElem = jArrayNext(qElem)) {
            if (jUint(jFind(qElem, "id")) == id)
                rpcError(qElem, errField);
        }
        rpcError(qHead, errField);
    }
}

/*
 * Run the call via `evm -x -n`, proxying its JSON-RPC requests through
 * the provided post function.  Collects account state into *accounts
 * and writes the output into r->output.
 */
static void runViaEvm(
    call_result_t *r,
    postFn         post, void *ctx,
    account_t    **accounts)
{
    strbuf_t cj = {0};
#define sbLit(s) sbAppend(&cj, s, sizeof(s) - 1)
#define sbStr(s) sbAppend(&cj, s, strlen(s))
    sbLit("{\"to\":\"");    sbStr(r->to);
    sbLit("\",\"from\":\""); sbStr(r->from);
    sbLit("\",\"data\":\""); sbStr(r->input);
    if (r->value[0]) { sbLit("\",\"value\":\""); sbStr(r->value); }
    sbLit("\"}");
#undef sbLit
#undef sbStr

    FILE *toChild, *fromChild;
    pid_t pid = spawnEvm(evmPath, cj.buf, &toChild, &fromChild);

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

            const char *elem1  = jArrayNext(elem0);
            const char *elem2  = jArrayNext(elem1);
            uint64_t codeId    = jUint(jFind(elem0, "id"));
            uint64_t nonceId   = jUint(jFind(elem1, "id"));
            uint64_t balanceId = jUint(jFind(elem2, "id"));

            char *resp = post(line, nl - line, ctx);
            if (!resp) rpcFailed(line);
            checkBatchErrors(resp, line);

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
                    id, r->block);
            fflush(toChild);

        } else if (strstr(line, "\"eth_getStorageAt\"")) {
            const char *params = jFind(line, "params");
            char addr[ADDR_LEN], rawKey[HEX256_LEN];
            const char *param0 = jArrayGet(params, 0);
            jStr(param0,             addr,   sizeof(addr));
            jStr(jArrayNext(param0), rawKey, sizeof(rawKey));
            account_t *acct = ensureAccount(accounts, addr);

            char *resp = post(line, nl - line, ctx);
            if (!resp) rpcFailed(line);
            const char *errField = jFind(resp, "error");
            if (errField) rpcError(line, errField);
            char val[HEX256_LEN];
            jStr(jFind(resp, "result"), val, sizeof(val));
            addStorage(acct, rawKey, val);
            fprintf(toChild, "%s\n", resp);
            fflush(toChild);
            free(resp);

        } else {
            /* ---- Final output from evm ---- */
            output = jStrDup(jFind(line, "returnData"));
            char gasHex[20];
            snprintf(gasHex, sizeof(gasHex), "0x%" PRIx64, jUint(jFind(line, "gasUsed")));
            r->gasUsed = strdup(gasHex);
            r->status  = jStrDup(jFind(line, "status"));
            r->logs    = jValDup(jFind(line, "logs"));
            break;
        }
    }

    fclose(toChild);
    fclose(fromChild);
    int status;
    waitpid(pid, &status, 0);
    free(line);
    free(cj.buf);

    if (!output || WEXITSTATUS(status) != 0) {
        fputs("dio: evm execution failed\n", stderr);
        _exit(1);
    }

    ensureAccount(accounts, r->to);
    if (strcmp(r->from, "0x0000000000000000000000000000000000000000") != 0)
        ensureAccount(accounts, r->from);

    r->output = output;
}

/*
 * Write the dio JSON config to outfile (stdout if NULL or "-").
 */
static void writeConfig(
    account_t     *accounts,
    call_result_t *results,
    const char    *outfile)
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

    fputs("    {\n        \"tests\": [\n", f);
    for (call_result_t *r = results; r; r = r->next) {
        if (r != results) fputs(",\n", f);
        fputs("            {\n", f);
        fprintf(f, "                \"to\": \"%s\"", r->to);
        if (strcmp(r->from, "0x0000000000000000000000000000000000000000") != 0)
            fprintf(f, ",\n                \"from\": \"%s\"", r->from);
        if (r->value[0])
            fprintf(f, ",\n                \"value\": \"%s\"", r->value);
        if (r->input && strcmp(r->input, "0x") != 0)
            fprintf(f, ",\n                \"input\": \"%s\"", r->input);
        fprintf(f, ",\n                \"blockNumber\": \"%s\"", r->block);
        if (r->gasUsed)
            fprintf(f, ",\n                \"gasUsed\": \"%s\"", r->gasUsed);
        if (r->logs)
            fprintf(f, ",\n                \"logs\": %s", r->logs);
        if (r->status)
            fprintf(f, ",\n                \"status\": \"%s\"", r->status);
        if (r->output)
            fprintf(f, ",\n                \"output\": \"%s\"", r->output);
        fputs("\n            }", f);
    }
    fputs("\n        ]\n    }\n]\n", f);

    if (f != stdout) {
        fclose(f);
        fprintf(stderr, "Wrote %s\n", outfile);
    }
}

/*
 * Parse callJson, resolve the block number, run via evm, and append the
 * result to *results.  Account state accumulates into *accounts.
 */
static void run(
    const char    *callJson,
    postFn         post, void *ctx,
    account_t    **accounts,
    call_result_t **results)
{
    call_result_t *r = malloc(sizeof(call_result_t));
    strcpy(r->to,    "0x0000000000000000000000000000000000000000");
    strcpy(r->from,  "0x0000000000000000000000000000000000000000");
    strcpy(r->block, "latest");
    r->value[0] = '\0';

    char tmp[ADDR_LEN + 2];
    if (jStr(jFind(callJson, "to"),   tmp, sizeof(tmp)) > 0) normalizeAddr(tmp, r->to);
    if (jStr(jFind(callJson, "from"), tmp, sizeof(tmp)) > 0) normalizeAddr(tmp, r->from);
    jStr(jFind(callJson, "value"), r->value, sizeof(r->value));

    const char *blkVal = jFind(callJson, "block");
    if (blkVal) jStr(blkVal, r->block, sizeof(r->block));

    const char *dataField = jFind(callJson, "data");
    if (!dataField) dataField = jFind(callJson, "input");
    r->input = jStrDup(dataField);
    if (r->input[0] != '0' || r->input[1] != 'x') {
        size_t ilen = strlen(r->input);
        char *t = malloc(ilen + 3);
        t[0] = '0'; t[1] = 'x';
        memcpy(t + 2, r->input, ilen + 1);
        free(r->input);
        r->input = t;
    }

    /* Resolve block number */
    if (strcmp(r->block, "latest") == 0) {
        static const char kBlockNumber[] =
            "{\"jsonrpc\":\"2.0\",\"id\":1,"
            "\"method\":\"eth_blockNumber\",\"params\":[]}";
        char *resp = post(kBlockNumber, sizeof(kBlockNumber) - 1, ctx);
        if (!resp) rpcFailed(kBlockNumber);
        const char *errField = jFind(resp, "error");
        if (errField) rpcError(kBlockNumber, errField);
        jStr(jFind(resp, "result"), r->block, sizeof(r->block));
        free(resp);
    }

    runViaEvm(r, post, ctx, accounts);

    r->next  = *results;
    *results = r;
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

    evmPath = findEvm(argv[0]);

    account_t     *accounts = NULL;
    call_result_t *results  = NULL;

    if (inlineJson) {
        run(inlineJson, post, ctx, &accounts, &results);
    } else if (optind < argc) {
        for (; optind < argc; optind++) {
            FILE *f = fopen(argv[optind], "r");
            if (!f) { perror(argv[optind]); return 1; }
            char *json = readAll(f);
            fclose(f);
            run(json, post, ctx, &accounts, &results);
            free(json);
        }
    } else {
        char *json = readAll(stdin);
        run(json, post, ctx, &accounts, &results);
        free(json);
    }

    writeConfig(accounts, results, outfile);
    return 0;
}
