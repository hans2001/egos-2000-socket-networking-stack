#include "app.h"
#include "servers.h"
#include <string.h>

static const char hello_response[] =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n"
    "\r\n"
    "EGOS socket abstraction demonstration\n";

int main(void) {
    int conn;
    int req_len;
    char req_buf[NET_BUF_SIZE];

    if (sock_listen(80) < 0) {
        INFO("http app failed to listen on port 80");
        return -1;
    }

    while (1) {
        conn = sock_accept();
        if (conn < 0) {
            continue;
        }

        req_len = sock_recv(conn, req_buf, sizeof(req_buf));
        if (req_len > 0) {
            sock_send(conn, hello_response, strlen(hello_response));
        }

        sock_close(conn);
    }

    return 0;
}
