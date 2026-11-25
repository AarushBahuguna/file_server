#include "ipc.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/* read exactly n bytes (messages are fixed size) */
static int read_full(int fd, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, (char *)buf + got, n - got);
        if (r == 0) return got == 0 ? 0 : -1;
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        got += (size_t)r;
    }
    return 1;
}

static int write_full(int fd, const void *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = write(fd, (const char *)buf + sent, n - sent);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        sent += (size_t)w;
    }
    return 0;
}

static void client_fifo_name(pid_t pid, char *out, size_t n) {
    snprintf(out, n, CLIENT_FIFO_FMT, (int)pid);
}

/* ================= server ================= */
int ipc_server_init(void) {
    if (mkfifo(SERVER_FIFO, 0666) < 0 && errno != EEXIST) { perror("mkfifo server"); return -1; }
    /* O_RDWR: server keeps a write end open itself, so read() never sees EOF
       when the last client disconnects, and open() does not block. */
    int fd = open(SERVER_FIFO, O_RDWR);
    if (fd < 0) perror("open server fifo");
    return fd;
}

int ipc_recv_request(int server_fd, request_t *req) {
    return read_full(server_fd, req, sizeof *req);
}

int ipc_send_response(pid_t client_pid, const response_t *resp) {
    char name[CLIENT_FIFO_MAX];
    client_fifo_name(client_pid, name, sizeof name);
    int fd = open(name, O_WRONLY | O_NONBLOCK);   /* fails (ENXIO) if client died */
    if (fd < 0) return -1;
    int rc = write_full(fd, resp, sizeof *resp);
    close(fd);
    return rc;
}

void ipc_server_close(int server_fd) {
    close(server_fd);
    unlink(SERVER_FIFO);
}

/* ================= client ================= */
static char g_reply_name[CLIENT_FIFO_MAX];
static int  g_reply_fd = -1;     /* opened O_RDWR so it never blocks / sees EOF */

int ipc_client_init(void) {
    client_fifo_name(getpid(), g_reply_name, sizeof g_reply_name);
    unlink(g_reply_name);
    if (mkfifo(g_reply_name, 0666) < 0) { perror("mkfifo client"); return -1; }
    g_reply_fd = open(g_reply_name, O_RDWR);
    if (g_reply_fd < 0) { perror("open client fifo"); return -1; }
    return 0;
}

int ipc_call(request_t *req, response_t *resp) {
    req->client_pid = getpid();
    int sfd = open(SERVER_FIFO, O_WRONLY | O_NONBLOCK);
    if (sfd < 0) {
        fprintf(stderr, "cannot reach server (%s): is it running?\n", strerror(errno));
        return -1;
    }
    int rc = write_full(sfd, req, sizeof *req);   /* single atomic write */
    close(sfd);
    if (rc < 0) return -1;
    return read_full(g_reply_fd, resp, sizeof *resp) == 1 ? 0 : -1;
}

void ipc_client_close(void) {
    if (g_reply_fd >= 0) close(g_reply_fd);
    unlink(g_reply_name);
}
