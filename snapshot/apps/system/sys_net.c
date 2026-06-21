/*
 * Adapted from the tcp-riscv course project.
 * Description: network system service for packet send/recv requests.
 */

#include "app.h"
#include <string.h>
#include <stdlib.h>

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

static int sock_listen_port = -1;
static int sock_conn_id = 1;
static int sock_conn_open = 0;
static int sock_conn_pending = 0;
static int sock_conn_closed = 0;

static char sock_rx_buf[NET_BUF_SIZE];
static int sock_rx_len = 0;

static char sock_tx_buf[NET_BUF_SIZE];
static int sock_tx_len = 0;
static int sock_tx_pending = 0;

static void send_net_reply(int sender, struct net_reply *reply, int status,
                           int conn_id, int length) {
    reply->status = status;
    reply->conn_id = conn_id;
    reply->length = length;
    grass->sys_send(sender, (void *)reply, sizeof(*reply));
}

static int clamp_net_length(int length) {
    if (length > NET_BUF_SIZE) {
        return NET_BUF_SIZE;
    }
    return length;
}

static int socket_conn_valid(int conn_id) {
    return sock_listen_port >= 0 && sock_conn_open && conn_id == sock_conn_id;
}

static void reset_socket_state(void) {
    sock_conn_id = 1;
    sock_conn_open = 0;
    sock_conn_pending = 0;
    sock_conn_closed = 0;
    sock_rx_len = 0;
    sock_tx_len = 0;
    sock_tx_pending = 0;

    httpd_socket_conn_open = 0;
    httpd_socket_conn_pending = 0;
    httpd_socket_conn_closed = 0;
    httpd_socket_rx_len = 0;
    httpd_socket_tx_len = 0;
    httpd_socket_tx_pending = 0;
    httpd_socket_close_pending = 0;
}

static void sync_socket_state(void) {
    sock_conn_open = httpd_socket_conn_open;
    sock_conn_pending = httpd_socket_conn_pending;
    sock_conn_closed = httpd_socket_conn_closed;
    sock_tx_pending = httpd_socket_tx_pending;

    if (httpd_socket_rx_len > 0) {
        sock_rx_len = httpd_socket_rx_len;
        if (sock_rx_len > NET_BUF_SIZE) {
            sock_rx_len = NET_BUF_SIZE;
        }
        memcpy(sock_rx_buf, httpd_socket_rx_buf, sock_rx_len);
        httpd_socket_rx_len = 0;
    }
}

static void kick_active_connection(void) {
    int i;

    if (sock_listen_port < 0) {
        return;
    }

    for (i = 0; i < UIP_CONNS; i++) {
        if (!uip_conn_active(i)) {
            continue;
        }
        if (uip_conns[i].lport != HTONS(sock_listen_port)) {
            continue;
        }

        uip_poll_conn(&uip_conns[i]);
        if (uip_len > 0) {
            uip_arp_out();
            earth->net_send(uip_len, (void *)uip_buf);
        }
        break;
    }

    sync_socket_state();
}

static void net_kick_tx(void) {
    if (!sock_tx_pending) {
        return;
    }
    kick_active_connection();
}

static void net_kick_close(void) {
    kick_active_connection();
}

static void net_poll(void) {
    uip_len = earth->net_recv((void *)uip_buf);
    if (uip_len <= 0) {
        return;
    }

    if (BUF->type == HTONS(UIP_ETHTYPE_IP)) {
        uip_arp_ipin();
        uip_input();
        if (uip_len > 0) {
            uip_arp_out();
            earth->net_send(uip_len, (void *)uip_buf);
        }
    } else if (BUF->type == HTONS(UIP_ETHTYPE_ARP)) {
        uip_arp_arpin();
        if (uip_len > 0) {
            earth->net_send(uip_len, (void *)uip_buf);
        }
    } else {
        uip_len = 0;
    }
    sync_socket_state();
}

static void handle_net_send(int sender, struct net_request *req,
                            struct net_reply *reply) {
    earth->net_send(req->length, (void *)req->buf);
    send_net_reply(sender, reply, NET_OK, NET_CONN_NONE, 0);
}

static void handle_net_recv(int sender, struct net_reply *reply) {
    reply->length = earth->net_recv(reply->buf);
    send_net_reply(sender, reply, NET_OK, NET_CONN_NONE, reply->length);
}

static void handle_sock_listen(int sender, struct net_request *req,
                               struct net_reply *reply) {
    if (req->port <= 0) {
        send_net_reply(sender, reply, NET_INVALID, NET_CONN_NONE, 0);
        return;
    }

    sock_listen_port = req->port;
    reset_socket_state();
    uip_listen(HTONS(req->port));
    INFO("sock_listen: port=%d", req->port);
    send_net_reply(sender, reply, NET_OK, NET_CONN_NONE, 0);
}

static void handle_sock_accept(int sender, struct net_reply *reply) {
    net_poll();
    if (sock_conn_pending) {
        sock_conn_pending = 0;
        httpd_socket_conn_pending = 0;
        INFO("sock_accept: conn=%d", sock_conn_id);
        send_net_reply(sender, reply, NET_OK, sock_conn_id, 0);
        return;
    }

    send_net_reply(sender, reply, NET_WOULD_BLOCK, NET_CONN_NONE, 0);
}

static void handle_sock_recv(int sender, struct net_request *req,
                             struct net_reply *reply) {
    net_poll();

    if (sock_listen_port < 0 || req->conn_id != sock_conn_id) {
        send_net_reply(sender, reply, NET_INVALID, req->conn_id, 0);
        return;
    }

    if (sock_rx_len > 0) {
        int rx_len = sock_rx_len;
        if (rx_len > req->length) {
            rx_len = req->length;
        }
        memcpy(reply->buf, sock_rx_buf, rx_len);
        sock_rx_len = 0;
        INFO("sock_recv: conn=%d len=%d", req->conn_id, rx_len);
        send_net_reply(sender, reply, NET_OK, req->conn_id, rx_len);
        return;
    }

    if (sock_conn_closed || !sock_conn_open) {
        send_net_reply(sender, reply, NET_CLOSED, req->conn_id, 0);
        return;
    }

    send_net_reply(sender, reply, NET_WOULD_BLOCK, req->conn_id, 0);
}

static void handle_sock_send(int sender, struct net_request *req,
                             struct net_reply *reply) {
    net_poll();

    if (sock_listen_port < 0 || !socket_conn_valid(req->conn_id)) {
        send_net_reply(sender, reply, NET_INVALID, req->conn_id, 0);
        return;
    }

    if (sock_tx_pending) {
        send_net_reply(sender, reply, NET_WOULD_BLOCK, req->conn_id, 0);
        return;
    }

    sock_tx_len = clamp_net_length(req->length);
    memcpy(sock_tx_buf, req->buf, sock_tx_len);
    memcpy(httpd_socket_tx_buf, sock_tx_buf, sock_tx_len);
    httpd_socket_tx_len = sock_tx_len;
    httpd_socket_tx_pending = 1;
    sock_tx_pending = 1;
    net_kick_tx();
    INFO("sock_send: conn=%d len=%d", req->conn_id, sock_tx_len);
    send_net_reply(sender, reply, NET_OK, req->conn_id, sock_tx_len);
}

static void handle_sock_close(int sender, struct net_request *req,
                              struct net_reply *reply) {
    net_poll();

    if (!socket_conn_valid(req->conn_id)) {
        send_net_reply(sender, reply, NET_INVALID, req->conn_id, 0);
        return;
    }

    httpd_socket_close_pending = 1;
    net_kick_close();
    sock_conn_open = 0;
    sock_conn_pending = 0;
    sock_tx_pending = 0;
    sock_rx_len = 0;
    INFO("sock_close: conn=%d", req->conn_id);
    send_net_reply(sender, reply, NET_OK, req->conn_id, 0);
}

int main() {      
    uip_ipaddr_t ipaddr;
    char buf[SYSCALL_MSG_LEN];
    struct uip_eth_addr ethaddr = {{0x52, 0x54, 0x00, 0x00, 0x00, 0x01}};

    SUCCESS("Enter kernel process GPID_NET");

    uip_init();
    uip_setethaddr(ethaddr);
    uip_arp_init();

    uip_ipaddr(ipaddr, 10, 0, 2, 15);
    uip_sethostaddr(ipaddr);

    uip_ipaddr(ipaddr, 10, 0, 2, 2);
    uip_setdraddr(ipaddr);

    uip_ipaddr(ipaddr, 255, 255, 255, 0);
    uip_setnetmask(ipaddr);

    strcpy(buf,"Finish GPID_NET initialization");
    grass->sys_send(GPID_PROCESS, buf, 32);

    while (1) {
        int sender;
        struct net_request* req = (void*)buf;
        struct net_reply* reply = (void*)buf;
        sender = 0;
        grass->sys_recv(GPID_ALL, &sender, buf, SYSCALL_MSG_LEN);

        switch (req->type) {
        case NET_SEND:
            handle_net_send(sender, req, reply);
            break;
        case NET_RECV:
            handle_net_recv(sender, reply);
            break;
        case NET_SOCK_LISTEN:
            handle_sock_listen(sender, req, reply);
            break;
        case NET_SOCK_ACCEPT:
            handle_sock_accept(sender, reply);
            break;
        case NET_SOCK_RECV:
            handle_sock_recv(sender, req, reply);
            break;
        case NET_SOCK_SEND:
            handle_sock_send(sender, req, reply);
            break;
        case NET_SOCK_CLOSE:
            handle_sock_close(sender, req, reply);
            break;
        default:
            FATAL("sys_net request[%d] not implemented", req->type);
        }
    }
}
