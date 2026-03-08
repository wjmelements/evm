#pragma once
#include <curl/curl.h>

/*
 * Connect to a ws:// or wss:// URL and perform the HTTP Upgrade handshake.
 * Returns the ready curl handle, or NULL on error (message to stderr).
 * Caller must curl_easy_cleanup() when done.
 */
CURL *wsConnect(const char *url);

/*
 * postFn-compatible: send payload as a WebSocket text frame and
 * return the response as a dynamically-allocated string.
 * ctx must be the CURL * returned by wsConnect.  Caller must free().
 */
char *wsPost(const char *payload, size_t len, void *ctx);
