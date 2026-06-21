#include "uip.h"
#include <string.h>

volatile int httpd_socket_conn_open = 0;
volatile int httpd_socket_conn_pending = 0;
volatile int httpd_socket_conn_closed = 0;
volatile int httpd_socket_rx_len = 0;
char httpd_socket_rx_buf[UIP_BUFSIZE];
volatile int httpd_socket_tx_len = 0;
volatile int httpd_socket_tx_pending = 0;
char httpd_socket_tx_buf[UIP_BUFSIZE];
volatile int httpd_socket_close_pending = 0;

void httpd_appcall(void) {
    if (uip_connected()) {
        httpd_socket_conn_open = 1;
        httpd_socket_conn_pending = 1;
        httpd_socket_conn_closed = 0;
    }

    if (uip_closed() || uip_aborted() || uip_timedout()) {
        httpd_socket_conn_open = 0;
        httpd_socket_conn_pending = 0;
        httpd_socket_conn_closed = 1;
    }

    if (uip_newdata()) {
        int rx_len = uip_datalen();
        if (rx_len > UIP_BUFSIZE) {
            rx_len = UIP_BUFSIZE;
        }
        memcpy(httpd_socket_rx_buf, uip_appdata, rx_len);
        httpd_socket_rx_len = rx_len;
    }

    if (uip_acked()) {
        httpd_socket_tx_pending = 0;
        httpd_socket_tx_len = 0;
    }

    if (httpd_socket_close_pending) {
        uip_close();
        httpd_socket_close_pending = 0;
    }

    if (httpd_socket_tx_pending &&
        (uip_poll() || uip_acked() || uip_rexmit() || uip_newdata())) {
        uip_send(httpd_socket_tx_buf, httpd_socket_tx_len);
    }
}

void httpd_init(void) {
}
