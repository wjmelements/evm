#include "ws.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* =========================================================
 * Internal helpers
 * ========================================================= */

/* xorshift32 PRNG for masking keys, seeded lazily from stack address + pid */
static uint32_t wsMask(void) {
    static uint32_t x = 0;
    if (!x) {
        x = (uint32_t)(uintptr_t)&x ^ (uint32_t)getpid();
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static int wsSendAll(CURL *curl, const void *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        size_t chunk;
        CURLcode rc;
        do {
            rc = curl_easy_send(curl, (const char *)buf + sent, n - sent, &chunk);
        }
        while (rc == CURLE_AGAIN);
        if (rc != CURLE_OK) {
            fprintf(stderr, "dio: ws send: %s\n", curl_easy_strerror(rc));
            return -1;
        }
        sent += chunk;
    }
    return 0;
}

static int wsRecvExact(CURL *curl, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        size_t chunk;
        CURLcode rc;
        do {
            rc = curl_easy_recv(curl, (char *)buf + got, n - got, &chunk);
        }
        while (rc == CURLE_AGAIN);
        if (rc != CURLE_OK) {
            fprintf(stderr, "dio: ws recv: %s\n", curl_easy_strerror(rc));
            return -1;
        }
        got += chunk;
    }
    return 0;
}

/* Append n bytes to a heap buffer, growing as needed, keeping it NUL-terminated. */
static void wsAppend(char **buf, size_t *len, size_t *cap, const void *data, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t nc = *cap ? *cap * 2 : 4096;
        while (nc < *len + n + 1) {
            nc *= 2;
        }
        *buf = realloc(*buf, nc);
        *cap = nc;
    }
    memcpy(*buf + *len, data, n);
    *len += n;
    (*buf)[*len] = '\0';
}

/* =========================================================
 * HTTP Upgrade handshake
 * ========================================================= */

/*
 * Perform the WebSocket HTTP Upgrade over an already-TCP-connected curl handle.
 * host: "hostname" or "hostname:port"; path: "/..." (must start with '/').
 */
static int wsHandshake(CURL *curl, const char *host, const char *path) {
    char req[4096];
    int n = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                     "Sec-WebSocket-Version: 13\r\n"
                     "Origin: null\r\n"
                     "\r\n",
                     path, host);
    if (wsSendAll(curl, req, (size_t)n) != 0) {
        return -1;
    }

    /* Read response until \r\n\r\n */
    char *rbuf = NULL;
    size_t rlen = 0, rcap = 0;
    char tmp[4096];
    while (!rbuf || !strstr(rbuf, "\r\n\r\n")) {
        size_t chunk;
        CURLcode rc;
        do {
            rc = curl_easy_recv(curl, tmp, sizeof(tmp), &chunk);
        }
        while (rc == CURLE_AGAIN);
        if (rc != CURLE_OK) {
            fprintf(stderr, "dio: ws handshake: %s\n", curl_easy_strerror(rc));
            free(rbuf);
            return -1;
        }
        wsAppend(&rbuf, &rlen, &rcap, tmp, chunk);
    }
    if (strncmp(rbuf, "HTTP/1.1 101", 12) != 0) {
        fprintf(stderr, "dio: ws handshake failed: %.80s\n", rbuf);
        free(rbuf);
        return -1;
    }
    free(rbuf);
    return 0;
}

/* =========================================================
 * Public API
 * ========================================================= */

CURL *wsConnect(const char *url) {
    int secure = strncmp(url, "wss://", 6) == 0;
    const char *s = url + (secure ? 6 : 5);

    char host[256], path[2048], httpUrl[2400];
    const char *slash = strchr(s, '/');
    if (slash) {
        snprintf(host, sizeof(host), "%.*s", (int)(slash - s), s);
        snprintf(path, sizeof(path), "%s", slash);
    } else {
        snprintf(host, sizeof(host), "%s", s);
        path[0] = '/';
        path[1] = '\0';
    }
    snprintf(httpUrl, sizeof(httpUrl), "%s://%s%s",
             secure ? "https" : "http", host, path);

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL,          httpUrl);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        fprintf(stderr, "dio: ws connect: %s\n", curl_easy_strerror(rc));
        curl_easy_cleanup(curl);
        return NULL;
    }
    if (wsHandshake(curl, host, path) != 0) {
        curl_easy_cleanup(curl);
        return NULL;
    }
    return curl;
}

/* Send a masked text frame (RFC 6455 §5.2). */
static int wsSendFrame(CURL *curl, const char *msg, size_t len) {
    uint8_t hdr[10];
    size_t hlen = 0;
    hdr[hlen++] = 0x81;                        /* FIN | opcode=text */
    if (len < 126) {
        hdr[hlen++] = 0x80 | (uint8_t)len;
    } else if (len < 65536) {
        hdr[hlen++] = 0x80 | 126;
        hdr[hlen++] = (uint8_t)(len >> 8);
        hdr[hlen++] = (uint8_t)len;
    } else {
        hdr[hlen++] = 0x80 | 127;
        for (int i = 7; i >= 0; i--) {
            hdr[hlen++] = (uint8_t)(len >> (i * 8));
        }
    }
    uint32_t m = wsMask();
    uint8_t mask[4] = {(uint8_t)m, (uint8_t)(m>>8), (uint8_t)(m>>16), (uint8_t)(m>>24)};
    hdr[hlen++] = mask[0];
    hdr[hlen++] = mask[1];
    hdr[hlen++] = mask[2];
    hdr[hlen++] = mask[3];
    if (wsSendAll(curl, hdr, hlen) != 0) {
        return -1;
    }

    char chunk[4096];
    size_t off = 0;
    while (off < len) {
        size_t n = len - off < sizeof(chunk) ? len - off : sizeof(chunk);
        for (size_t i = 0; i < n; i++) {
            chunk[i] = (char)((uint8_t)msg[off + i] ^ mask[(off + i) & 3]);
        }
        if (wsSendAll(curl, chunk, n) != 0) {
            return -1;
        }
        off += n;
    }
    return 0;
}

/* Receive a complete WebSocket message, reassembling continuation frames. */
static char *wsRecvMsg(CURL *curl) {
    char *msg = NULL;
    size_t mlen = 0, mcap = 0;
    for (;;) {
        uint8_t hdr[2];
        if (wsRecvExact(curl, hdr, 2) != 0) {
            free(msg);
            return NULL;
        }
        int fin    = hdr[0] & 0x80;
        int opcode = hdr[0] & 0x0f;
        uint64_t plen = hdr[1] & 0x7f;
        if (plen == 126) {
            uint8_t ext[2];
            if (wsRecvExact(curl, ext, 2) != 0) {
                free(msg);
                return NULL;
            }
            plen = ((uint64_t)ext[0] << 8) | ext[1];
        } else if (plen == 127) {
            uint8_t ext[8];
            if (wsRecvExact(curl, ext, 8) != 0) {
                free(msg);
                return NULL;
            }
            plen = 0;
            for (int i = 0; i < 8; i++) {
                plen = (plen << 8) | ext[i];
            }
        }
        char *payload = malloc(plen + 1);
        if (wsRecvExact(curl, payload, plen) != 0) {
            free(payload);
            free(msg);
            return NULL;
        }
        payload[plen] = '\0';
        if (opcode == 0x08) {
            free(payload);
            free(msg);
            return NULL;
        }                                                              /* Close */
        if (opcode == 0x09) {                                           /* Ping → Pong */
            uint8_t pong[2] = {0x8a, 0x00};
            wsSendAll(curl, pong, 2);
            free(payload);
            continue;
        }
        wsAppend(&msg, &mlen, &mcap, payload, (size_t)plen);
        free(payload);
        if (fin) {
            break;
        }
    }
    return msg ? msg : strdup("");
}

char *wsPost(const char *payload, size_t len, void *ctx) {
    CURL *curl = ctx;
    if (wsSendFrame(curl, payload, len) != 0) {
        return NULL;
    }
    return wsRecvMsg(curl);
}

void wsClose(CURL *curl) {
    /* RFC 6455 §5.5.1: masked close frame, status 1000 (normal closure) */
    uint32_t m = wsMask();
    uint8_t frame[8] = {
        0x88,                          /* FIN | opcode=close */
        0x82,                          /* MASK | payload len=2 */
        (uint8_t)m,  (uint8_t)(m>>8), (uint8_t)(m>>16), (uint8_t)(m>>24),
        (uint8_t)(0x03 ^ (uint8_t)m),
        (uint8_t)(0xe8 ^ (uint8_t)(m>>8)),
    };
    wsSendAll(curl, frame, sizeof(frame));
    curl_easy_cleanup(curl);
}
