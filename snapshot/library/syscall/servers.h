#pragma once

void exit(int status);
int term_read(char* buf, uint len);
void term_write(char* str, uint len);
int dir_lookup(int dir_ino, char* name);
int file_read(int file_ino, uint offset, uint len, char* buf);
int file_write(int file_ino, uint offset, uint len, char* buf);
int file_getsize(int file_ino, uint* fsize);

int net_send(int length, char* packet);
int net_recv(char* packet);
int sock_listen(int port);
int sock_accept(void);
int sock_recv(int conn_id, char* buf, int len);
int sock_send(int conn_id, const char* buf, int len);
int sock_close(int conn_id);

enum grass_servers {
    GPID_ALL = -1,
    GPID_UNUSED,    /* 0 */
    GPID_PROCESS,   /* 1 */
    GPID_TERMINAL,  /* 2 */
    GPID_FILE,      /* 3 */
    GPID_NET,       /* 4 */
    GPID_SHELL,     /* 5 */
    GPID_USER_START /* 6 */
};

/* GPID_PROCESS */
#define CMD_NARGS   16
#define CMD_ARG_LEN 32

struct proc_request {
    enum { PROC_SPAWN, PROC_EXIT, PROC_KILLALL } type;
    int argc;
    char argv[CMD_NARGS][CMD_ARG_LEN];
};

struct proc_reply {
    enum { CMD_OK, CMD_ERROR } type;
};

/* GPID_TERMINAL */
#define TERM_BUF_SIZE 1024
struct term_request {
    enum { TERM_INPUT, TERM_OUTPUT } type;
    uint len;
    char buf[TERM_BUF_SIZE];
};

struct term_reply {
    uint len;
    char buf[TERM_BUF_SIZE];
};

/* GPID_FILE */
#include "disk.h"
struct file_request {
    enum {
        FILE_UNUSED,
        FILE_READ,
        FILE_WRITE,
        FILE_GETSIZE,
        DIR_LOOKUP,
    } type;
    uint ino;
    uint offset;
    uint len;
    block_t block;
};

struct file_reply {
    enum file_status { FILE_OK, FILE_ERROR } status;
    block_t block;
};

/* GPID_NET */
#define NET_BUF_SIZE 512
#define NET_CONN_NONE (-1)

enum net_request_type {
    NET_SEND,
    NET_RECV,
    NET_SOCK_LISTEN,
    NET_SOCK_ACCEPT,
    NET_SOCK_RECV,
    NET_SOCK_SEND,
    NET_SOCK_CLOSE,
};

enum net_status {
    NET_OK,
    NET_ERROR,
    NET_WOULD_BLOCK,
    NET_CLOSED,
    NET_INVALID,
};

struct net_request {
    enum net_request_type type;
    int port;
    int conn_id;
    int length;
    char buf[NET_BUF_SIZE];
};

struct net_reply {
    enum net_status status;
    int conn_id;
    int length;
    char buf[NET_BUF_SIZE];
};
