/* dummy_server.c - canned-response server for testing the CLI (Shubham).
 * Any path starting with "missing" gives ST_ERR_NOENT. Stop it with: bin/echo_client --shutdown */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "ipc.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sig(int s) { (void)s; g_stop = 1; }

static const char FILE_DATA[] = "dummy data from the dummy server";

int main(void) {
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
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
        if (strncmp(req.path, "missing", 7) == 0 &&
            (req.cmd == CMD_CD || req.cmd == CMD_LS || req.cmd == CMD_OPEN || req.cmd == CMD_DELETE)) {
            rs.status = ST_ERR_NOENT;
        } else {
            switch (req.cmd) {
            case CMD_LOGIN: case CMD_LOGOUT: case CMD_MKDIR: case CMD_CREATE:
            case CMD_CLOSE: case CMD_DELETE:
                break;
            case CMD_CD:
                rs.length = (uint32_t)snprintf(rs.data, sizeof rs.data, "%s", req.path);
                break;
            case CMD_LS:
                rs.length = (uint32_t)snprintf(rs.data, sizeof rs.data, "a.txt\nnotes/\n");
                break;
            case CMD_OPEN:
                rs.fd = 3;
                break;
            case CMD_READ: {
                uint32_t size = (uint32_t)strlen(FILE_DATA);
                if (req.offset < size) {
                    uint32_t n = size - req.offset;
                    if (n > req.length) n = req.length;
                    memcpy(rs.data, FILE_DATA + req.offset, n);
                    rs.length = n;
                }
                break;
            }
            case CMD_WRITE:
                rs.length = req.length;
                break;
            case CMD_STATS:
                rs.length = (uint32_t)snprintf(rs.data, sizeof rs.data, "dummy stats: nothing real yet\n");
                break;
            default:
                rs.status = ST_ERR_INVAL;
            }
        }
        ipc_send_response(req.client_pid, &rs);
    }
    ipc_server_close(sfd);
    return 0;
}
