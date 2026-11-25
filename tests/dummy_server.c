/* dummy_server.c - canned-response server for testing the CLI (Shubham).
 * Uses the real session and log modules. Any path containing "missing" gives ST_ERR_NOENT.
 * Stop it with: bin/echo_client --shutdown     Log: /tmp/dummy_server.log */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "ipc.h"
#include "session.h"
#include "log.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sig(int s) { (void)s; g_stop = 1; }

static const char FILE_DATA[] = "dummy data from the dummy server";

static void handle(const request_t *req, response_t *rs) {
    pid_t pid = req->client_pid;
    char abs[PROTO_MAX_PATH];
    sess_file_t f;
    int fd;

    if (req->cmd == CMD_LOGIN) { rs->status = session_login(pid, req->user); return; }
    if (req->cmd == CMD_LOGOUT) {
        sess_file_t left[SESS_MAX_FDS];
        int n = 0;
        rs->status = session_logout(pid, left, SESS_MAX_FDS, &n);
        if (n) LOG_W("session", "%s logged out with %d file(s) still open", req->user, n);
        return;
    }

    char u[PROTO_MAX_USER];
    if (session_user(pid, u, sizeof u) != ST_OK) { rs->status = ST_ERR_PERM; return; }   /* not logged in */

    switch (req->cmd) {
    case CMD_CD:
        if ((rs->status = session_resolve(pid, req->path, abs, sizeof abs)) != ST_OK) return;
        if (strstr(abs, "missing")) { rs->status = ST_ERR_NOENT; return; }
        session_chdir(pid, abs);
        rs->length = (uint32_t)snprintf(rs->data, sizeof rs->data, "%s", abs);
        break;
    case CMD_MKDIR: case CMD_CREATE: case CMD_DELETE: case CMD_LS:
    case CMD_OPEN:
        if ((rs->status = session_resolve(pid, req->path, abs, sizeof abs)) != ST_OK) return;
        if (strstr(abs, "missing")) { rs->status = ST_ERR_NOENT; return; }
        if (req->cmd == CMD_LS)
            rs->length = (uint32_t)snprintf(rs->data, sizeof rs->data, "a.txt\nnotes/\n");
        else if (req->cmd == CMD_OPEN) {
            if ((rs->status = session_fd_open(pid, abs, req->mode, &fd)) == ST_OK) rs->fd = fd;
        }
        break;
    case CMD_CLOSE:
        rs->status = session_fd_close(pid, req->fd);
        break;
    case CMD_READ: {
        if ((rs->status = session_fd_get(pid, req->fd, &f)) != ST_OK) return;
        uint32_t size = (uint32_t)strlen(FILE_DATA);
        if (req->offset < size) {
            uint32_t n = size - req->offset;
            if (n > req->length) n = req->length;
            memcpy(rs->data, FILE_DATA + req->offset, n);
            rs->length = n;
        }
        break;
    }
    case CMD_WRITE:
        if ((rs->status = session_fd_get(pid, req->fd, &f)) != ST_OK) return;
        if (f.mode != OPEN_WRITE) { rs->status = ST_ERR_PERM; return; }
        rs->length = req->length;
        break;
    case CMD_STATS: {
        int n = snprintf(rs->data, sizeof rs->data, "dummy stats: nothing real yet\n");
        session_summary(rs->data + n, sizeof rs->data - (size_t)n);
        rs->length = (uint32_t)strlen(rs->data);
        break;
    }
    default:
        rs->status = ST_ERR_INVAL;
    }
}

int main(void) {
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    log_init("/tmp/dummy_server.log", LOG_INFO, 0);
    int sfd = ipc_server_init();
    if (sfd < 0) return 1;
    printf("[dummy_server] listening\n");
    fflush(stdout);

    request_t req;
    while (!g_stop && ipc_recv_request(sfd, &req) > 0) {
        response_t rs;
        memset(&rs, 0, sizeof rs);
        rs.req_id = req.req_id;
        rs.status = ST_OK;

        if (req.cmd == CMD_LOGOUT && strcmp(req.path, "__shutdown__") == 0) {
            ipc_send_response(req.client_pid, &rs);
            break;
        }
        handle(&req, &rs);
        log_request(&req, &rs, 0.0);
        ipc_send_response(req.client_pid, &rs);
    }
    ipc_server_close(sfd);
    log_close();
    return 0;
}
