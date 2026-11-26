#include "ipc.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int g_client_socket = -1;
static int g_timeout_ms = 30000;

static int recv_full_fd(int fd, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t rc = recv(fd, (char *)buf + got, n - got, 0);
        if (rc == 0) return got == 0 ? 0 : -1;
        if (rc < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        got += (size_t)rc;
    }
    return 1;
}

static int send_full_fd(int fd, const void *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t rc = send(fd, (const char *)buf + sent, n - sent, 0);
        if (rc < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)rc;
    }
    return 0;
}

int tcp_server_init(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);

    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 32) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int tcp_server_accept(int listen_fd) {
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    return accept(listen_fd, (struct sockaddr *)&peer, &peer_len);
}

int tcp_recv_request(int client_fd, request_t *req) {
    return recv_full_fd(client_fd, req, sizeof(*req));
}

int tcp_send_response(int client_fd, const response_t *resp) {
    return send_full_fd(client_fd, resp, sizeof(*resp));
}

void tcp_server_close(int listen_fd) {
    if (listen_fd >= 0) close(listen_fd);
}

int tcp_client_init(const char *host, int port) {
    if (!host || !*host) host = "127.0.0.1";

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1) {
        close(fd);
        errno = EINVAL;
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(fd);
        return -1;
    }

    g_client_socket = fd;
    return 0;
}

int tcp_client_call(request_t *req, response_t *resp) {
    if (g_client_socket < 0) {
        errno = ENOTCONN;
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = g_timeout_ms > 0 ? g_timeout_ms / 1000 : 30;
    timeout.tv_usec = g_timeout_ms > 0 ? (g_timeout_ms % 1000) * 1000 : 0;
    setsockopt(g_client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    req->client_pid = getpid();
    if (send_full_fd(g_client_socket, req, sizeof(*req)) < 0) return -1;

    int rc = recv_full_fd(g_client_socket, resp, sizeof(*resp));
    if (rc == 0) {
        errno = ENOTCONN;
        return -1;
    }
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) errno = ETIMEDOUT;
        return -1;
    }
    if (resp->req_id != req->req_id) {
        errno = EBADMSG;
        return -1;
    }
    return 0;
}

void tcp_client_close(void) {
    if (g_client_socket >= 0) {
        shutdown(g_client_socket, SHUT_RDWR);
        close(g_client_socket);
        g_client_socket = -1;
    }
}
