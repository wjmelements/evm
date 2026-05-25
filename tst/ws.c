#include "ws.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static const char upgrade_response[] =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
    "\r\n";

/* Accept one connection, complete WebSocket upgrade, read and verify the
 * close frame sent by wsClose(). */
static void run_server(int srv) {
    int fd = accept(srv, NULL, NULL);
    assert(fd >= 0);

    // drain the HTTP upgrade request
    char buf[4096] = {0};
    size_t n = 0;
    while (!strstr(buf, "\r\n\r\n") && n < sizeof(buf) - 1) {
        ssize_t r = read(fd, buf + n, (sizeof(buf) - n - 1));
        assert(r > 0);
        n += r;
    }
    write(fd, upgrade_response, sizeof(upgrade_response) - 1);

    // read the close frame
    uint8_t frame[8];
    size_t got = 0;
    while (got < 8) {
        ssize_t r = read(fd, frame + got, sizeof(frame) - got);
        assert(r > 0);
        got += r;
    }

    assert(frame[0] == 0x88);           /* FIN | opcode=close */
    assert((frame[1] & 0x80) != 0);     /* MASK bit set (client must mask) */
    assert((frame[1] & 0x7f) == 2);     /* payload length = 2 */

    // unmask and verify status code 1000 (0x03E8)
    uint8_t *mask = frame + 2;
    assert((frame[6] ^ mask[0]) == 0x03);
    assert((frame[7] ^ mask[1]) == 0xe8);

    // expect EOF
    got = read(fd, frame, 1);
    assert(got == 0);

    close(fd);
    close(srv);
}

// https://datatracker.ietf.org/doc/html/rfc6455#section-5.5.1
void test_wsClose(void) {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    assert(srv >= 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr_in = {0};
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr_in.sin_port = 0;
    struct sockaddr *addr = (struct sockaddr *)&addr_in;
    socklen_t alen = sizeof(addr_in);

    assert(bind(srv, addr, alen) == 0);
    assert(listen(srv, 1) == 0);

    getsockname(srv, addr, &alen);
    int port = ntohs(addr_in.sin_port);

    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        run_server(srv);
        exit(0);
    }
    close(srv);

    char url[64];
    snprintf(url, sizeof(url), "ws://127.0.0.1:%d/", port);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = wsConnect(url);
    assert(curl != NULL);
    wsClose(curl);
    curl_global_cleanup();

    int status;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(void) {
    test_wsClose();
    return 0;
}
