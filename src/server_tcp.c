#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ipc.h"
#include "protocol.h"
#include "threadpool.h"
#include "dispatcher.h"
#include "rwlock.h"
#include "cache.h"
#include "sched.h"
#include "disk.h"
#include "fs.h"
#include "log.h"

static volatile sig_atomic_t g_running = 1;
static pthread_t g_accept_thread;
static unsigned int g_tcp_session_counter = 100000;

struct tcp_connection {
    int socket_fd;
};

static void tcp_stop_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static void *tcp_connection_thread(void *arg) {
    struct tcp_connection *connection = (struct tcp_connection *)arg;
    int socket_fd = connection->socket_fd;
    pid_t network_pid = (pid_t)++g_tcp_session_counter;
    free(connection);

    while (g_running) {
        request_t req;
        int rc = tcp_recv_request(socket_fd, &req);
        if (rc == 0) break;
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }
req.client_pid = network_pid;

        response_t rs;
        if (dispatch_request_sync(&req, &rs) != 0) {
            memset(&rs, 0, sizeof(rs));
            rs.req_id = req.req_id;
            rs.status = ST_ERR_INTERNAL;
        }
        tcp_send_response(socket_fd, &rs);
    }

    shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);
    return NULL;
}

static void *tcp_accept_loop(void *arg) {
    int listen_fd = *(int *)arg;
    free(arg);

    while (g_running) {
        int client_fd = tcp_server_accept(listen_fd);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (!g_running) break;
            continue;
        }

        struct tcp_connection *connection = calloc(1, sizeof(*connection));
        if (!connection) {
            close(client_fd);
            continue;
        }
        connection->socket_fd = client_fd;

        pthread_t worker;
        if (pthread_create(&worker, NULL, tcp_connection_thread, connection) != 0) {
            close(client_fd);
            free(connection);
            continue;
        }
        pthread_detach(worker);
    }
    return NULL;
}

int tcp_server_main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tcp_stop_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    disk_init();
    if (disk_load("disk.img") != 0) fs_format(); else fs_mount();
    cache_init(); sched_init(); lock_manager_init(); dispatcher_init();
    log_init("logs/server.log", LOG_INFO, 0);
    if (threadpool_init(MAX_WORKERS) < 0) return -1;

    const char *port_text = getenv("FS_SERVER_PORT");
    int port = port_text ? atoi(port_text) : TCP_DEFAULT_PORT;
    int listen_fd = tcp_server_init(port);
    if (listen_fd < 0) {
        perror("tcp_server_init");
        return -1;
    }

    int *listen_copy = malloc(sizeof(*listen_copy));
    if (!listen_copy) {
        tcp_server_close(listen_fd);
        return -1;
    }
    *listen_copy = listen_fd;

    if (pthread_create(&g_accept_thread, NULL, tcp_accept_loop, listen_copy) != 0) {
        tcp_server_close(listen_fd);
        free(listen_copy);
        return -1;
    }

    printf("TCP file server listening on port %d\n", port);
    while (g_running) pause();

    pthread_join(g_accept_thread, NULL);
    tcp_server_close(listen_fd);
    threadpool_shutdown();
    cache_flush(); disk_save("disk.img");
    cache_destroy(); sched_shutdown(); lock_manager_destroy(); fs_unmount();
    log_close();
    return 0;
}
