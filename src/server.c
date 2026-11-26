#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "server.h"
#include "threadpool.h"
#include "dispatcher.h"
#include "rwlock.h"
#include "cache.h"
#include "sched.h"
#include "disk.h"
#include "fs.h"
#include "ipc.h"
#include "log.h"

static volatile sig_atomic_t running = 1;
static void handle_signal(int sig) { (void)sig; running = 0; }

int server_start(void) {
    struct sigaction sa; memset(&sa, 0, sizeof(sa)); sa.sa_handler = handle_signal; sigemptyset(&sa.sa_mask); sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL); signal(SIGPIPE, SIG_IGN);

    disk_init();
    if (disk_load("disk.img") != 0) fs_format(); else fs_mount();
    cache_init(); sched_init(); lock_manager_init(); dispatcher_init();
    log_init("logs/server.log", LOG_INFO, 0);
    if (threadpool_init(MAX_WORKERS) < 0) return -1;

    int fd = ipc_server_init();
    if (fd < 0) { threadpool_shutdown(); return -1; }

    while (running) {
        request_t req;
        int rc = ipc_recv_request(fd, &req);
        if (rc == 0) continue;
        if (rc < 0) { if (errno == EINTR) continue; break; }
        if (threadpool_submit(&req) < 0) break;
    }

    threadpool_shutdown();
    cache_flush();
    disk_save("disk.img");
    cache_destroy(); sched_shutdown(); lock_manager_destroy(); fs_unmount();
    log_close(); ipc_server_close(fd);
    return 0;
}

void server_stop(void) { running = 0; }
int main(void) {
    const char *transport = getenv("FS_TRANSPORT");
    if (transport && strcmp(transport, "tcp") == 0)
        return tcp_server_main();
    return server_start();
}
