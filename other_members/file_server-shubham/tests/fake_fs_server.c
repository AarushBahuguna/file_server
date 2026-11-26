/* fake_fs_server.c - tiny in-memory file system server (Shubham).
 * Lets the CLI and the ops scripts run before the real server exists. It speaks the real protocol,
 * uses the real session/log/stats modules, and keeps files in RAM (max 8 KB each).
 * Stops cleanly on SIGTERM/SIGINT.   Log: /tmp/fake_fs_server.log */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include "ipc.h"
#include "session.h"
#include "log.h"
#include "stats.h"

#define MAX_NODES   128
#define MAX_FILE    8192
#define DISK_BLOCKS 1024
#define BLOCK       512

typedef struct {
    int used, is_dir;
    char path[PROTO_MAX_PATH];
    uint32_t size;
    char data[MAX_FILE];
} node_t;

static node_t g_nodes[MAX_NODES];
static volatile sig_atomic_t g_stop = 0;
static unsigned long g_requests = 0;
static time_t g_start;

static void on_sig(int s) { (void)s; g_stop = 1; }

static node_t *find(const char *path) {
    for (int i = 0; i < MAX_NODES; i++)
        if (g_nodes[i].used && !strcmp(g_nodes[i].path, path)) return &g_nodes[i];
    return NULL;
}

static void parent_of(const char *path, char *out, size_t n) {
    snprintf(out, n, "%s", path);
    char *s = strrchr(out, '/');
    if (s == out) out[1] = '\0'; else *s = '\0';
}

static int dir_exists(const char *path) {
    if (!strcmp(path, "/")) return 1;
    node_t *x = find(path);
    return x && x->is_dir;
}

static int has_children(const char *dir) {
    char par[PROTO_MAX_PATH];
    for (int i = 0; i < MAX_NODES; i++) {
        if (!g_nodes[i].used) continue;
        parent_of(g_nodes[i].path, par, sizeof par);
        if (!strcmp(par, dir)) return 1;
    }
    return 0;
}

static int add_node(const char *path, int is_dir) {
    char par[PROTO_MAX_PATH];
    parent_of(path, par, sizeof par);
    if (!dir_exists(par)) return ST_ERR_NOENT;
    if (!strcmp(path, "/") || find(path)) return ST_ERR_EXIST;
    for (int i = 0; i < MAX_NODES; i++) {
        if (g_nodes[i].used) continue;
        memset(&g_nodes[i], 0, sizeof g_nodes[i]);
        g_nodes[i].used = 1;
        g_nodes[i].is_dir = is_dir;
        snprintf(g_nodes[i].path, sizeof g_nodes[i].path, "%s", path);
        return ST_OK;
    }
    return ST_ERR_NOSPC;
}

static void do_stats(response_t *rs) {
    unsigned files = 0, dirs = 0, used = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        if (!g_nodes[i].used) continue;
        if (g_nodes[i].is_dir) dirs++;
        else { files++; used += (g_nodes[i].size + BLOCK - 1) / BLOCK; }
    }
    char *b = rs->data;
    size_t cap = sizeof rs->data, len = 0;
    stats_put_u64(b, cap, &len, "disk.total_blocks", DISK_BLOCKS);
    stats_put_u64(b, cap, &len, "disk.block_size", BLOCK);
    stats_put_u64(b, cap, &len, "disk.used_blocks", used);
    stats_put_u64(b, cap, &len, "disk.free_blocks", DISK_BLOCKS - used);
    stats_put_u64(b, cap, &len, "disk.files", files);
    stats_put_u64(b, cap, &len, "disk.dirs", dirs);
    stats_put_u64(b, cap, &len, "server.requests", g_requests);
    stats_put_u64(b, cap, &len, "server.uptime_s", (unsigned long long)(time(NULL) - g_start));
    stats_put_u64(b, cap, &len, "sessions.active", (unsigned long long)session_count());
    rs->length = (uint32_t)len;
}

static void handle(const request_t *req, response_t *rs) {
    pid_t pid = req->client_pid;
    char abs[PROTO_MAX_PATH], u[PROTO_MAX_USER];
    sess_file_t f;
    node_t *nd;
    int fd;

    if (req->cmd == CMD_LOGIN) { rs->status = session_login(pid, req->user); return; }
    if (req->cmd == CMD_LOGOUT) { rs->status = session_logout(pid, NULL, 0, NULL); return; }
    if (session_user(pid, u, sizeof u) != ST_OK) { rs->status = ST_ERR_PERM; return; }

    switch (req->cmd) {
    case CMD_MKDIR: case CMD_CREATE:
        if ((rs->status = session_resolve(pid, req->path, abs, sizeof abs)) != ST_OK) return;
        rs->status = add_node(abs, req->cmd == CMD_MKDIR);
        break;
    case CMD_CD:
        if ((rs->status = session_resolve(pid, req->path, abs, sizeof abs)) != ST_OK) return;
        if (!dir_exists(abs)) { rs->status = ST_ERR_NOENT; return; }
        session_chdir(pid, abs);
        rs->length = (uint32_t)snprintf(rs->data, sizeof rs->data, "%s", abs);
        break;
    case CMD_LS: {
        if ((rs->status = session_resolve(pid, req->path, abs, sizeof abs)) != ST_OK) return;
        if (!dir_exists(abs)) { rs->status = ST_ERR_NOENT; return; }
        size_t len = 0;
        char par[PROTO_MAX_PATH];
        for (int i = 0; i < MAX_NODES; i++) {
            if (!g_nodes[i].used) continue;
            parent_of(g_nodes[i].path, par, sizeof par);
            if (strcmp(par, abs)) continue;
            const char *name = strrchr(g_nodes[i].path, '/') + 1;
            int w = snprintf(rs->data + len, sizeof rs->data - len, "%s%s\n", name, g_nodes[i].is_dir ? "/" : "");
            if (w < 0 || (size_t)w >= sizeof rs->data - len) break;
            len += (size_t)w;
        }
        rs->length = (uint32_t)len;
        break;
    }
    case CMD_DELETE:
        if ((rs->status = session_resolve(pid, req->path, abs, sizeof abs)) != ST_OK) return;
        if (!(nd = find(abs))) { rs->status = ST_ERR_NOENT; return; }
        if (nd->is_dir && has_children(abs)) { rs->status = ST_ERR_INVAL; return; }   /* directory not empty */
        nd->used = 0;
        break;
    case CMD_OPEN:
        if ((rs->status = session_resolve(pid, req->path, abs, sizeof abs)) != ST_OK) return;
        if (!(nd = find(abs))) { rs->status = ST_ERR_NOENT; return; }
        if (nd->is_dir) { rs->status = ST_ERR_INVAL; return; }
        if ((rs->status = session_fd_open(pid, abs, req->mode, &fd)) == ST_OK) rs->fd = fd;
        break;
    case CMD_CLOSE:
        rs->status = session_fd_close(pid, req->fd);
        break;
    case CMD_READ:
        if ((rs->status = session_fd_get(pid, req->fd, &f)) != ST_OK) return;
        if (!(nd = find(f.path))) { rs->status = ST_ERR_BADFD; return; }
        if (req->offset < nd->size) {
            uint32_t n = nd->size - req->offset;
            if (n > req->length) n = req->length;
            if (n > PROTO_MAX_RESP_DATA) n = PROTO_MAX_RESP_DATA;
            memcpy(rs->data, nd->data + req->offset, n);
            rs->length = n;
        }
        break;
    case CMD_WRITE:
        if ((rs->status = session_fd_get(pid, req->fd, &f)) != ST_OK) return;
        if (f.mode != OPEN_WRITE) { rs->status = ST_ERR_PERM; return; }
        if (!(nd = find(f.path))) { rs->status = ST_ERR_BADFD; return; }
        if (req->length > PROTO_MAX_REQ_DATA || (uint64_t)req->offset + req->length > MAX_FILE) { rs->status = ST_ERR_NOSPC; return; }
        if (req->offset > nd->size) memset(nd->data + nd->size, 0, req->offset - nd->size);
        memcpy(nd->data + req->offset, req->data, req->length);
        if (req->offset + req->length > nd->size) nd->size = req->offset + req->length;
        rs->length = req->length;
        break;
    case CMD_STATS:
        do_stats(rs);
        break;
    default:
        rs->status = ST_ERR_INVAL;
    }
}

int main(void) {
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    g_start = time(NULL);
    log_init("/tmp/fake_fs_server.log", LOG_INFO, 0);
    int sfd = ipc_server_init();
    if (sfd < 0) return 1;
    printf("[fake_fs_server] listening\n");
    fflush(stdout);

    struct pollfd p = { .fd = sfd, .events = POLLIN };
    request_t req;
    while (!g_stop) {
        int r = poll(&p, 1, 200);
        if (r <= 0) continue;
        if (ipc_recv_request(sfd, &req) <= 0) break;
        response_t rs;
        memset(&rs, 0, sizeof rs);
        rs.req_id = req.req_id;
        rs.status = ST_OK;
        g_requests++;
        handle(&req, &rs);
        log_request(&req, &rs, 0.0);
        ipc_send_response(req.client_pid, &rs);
    }
    ipc_server_close(sfd);
    log_close();
    return 0;
}
