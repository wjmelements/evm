#include "json.h"
#include <stdlib.h>
#include <string.h>

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

const char *jNextKeyVal(const char *p, const char **keyp, size_t *keylen, const char **valp) {
    while (*p && *p != '"' && *p != '}') p++;
    if (!*p || *p == '}') return NULL;
    p++;
    *keyp = p;
    while (*p && *p != '"') p++;
    if (!*p) return NULL;
    *keylen = p - *keyp;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p++ != ':') return NULL;
    while (*p == ' ' || *p == '\t') p++;
    *valp = p;
    jSkip(&p);
    return p;
}

const char *jFind(const char *p, const char *key) {
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

int jStr(const char *p, char *buf, size_t buflen) {
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

uint64_t jUint(const char *p) {
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

const char *jArrayGet(const char *p, int n) {
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

char *jValDup(const char *p) {
    if (!p) return NULL;
    const char *start = p;
    jSkip(&p);
    size_t len = p - start;
    char *s = malloc(len + 1);
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

char *jStrDup(const char *p) {
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

const char *jArrayNext(const char *p) {
    jSkip(&p);
    skipWs(&p);
    if (*p == ',') p++;
    skipWs(&p);
    if (!*p || *p == ']') return NULL;
    return p;
}

char *resultById(const char *resp, uint64_t targetId) {
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
