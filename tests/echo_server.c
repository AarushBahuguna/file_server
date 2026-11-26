/* echo_server.c - Day 2 dummy server (Shubham). Echoes request data back.
 * Not the real server (that is Shwetank's); it only tests the IPC layer. */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "ipc.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sig(int s) { (void)s; g_stop = 1; }

int main(void) {
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    int sfd = ipc_server_init();
    if (sfd < 0) return 1;
    printf("[echo_server] listening on %s\n", SERVER_FIFO);
    fflush(stdout);

    unsigned long served = 0;
    request_t req;
    while (!g_stop) {
        int r = ipc_recv_request(sfd, &req);
        if (r <= 0) break;                       /* EOF, error or signal */

        /* special request used by the test script to stop the server */
        if (req.cmd == CMD_LOGOUT && strcmp(req.path, "__shutdown__") == 0) {
            printf("[echo_server] shutdown requested\n");
            response_t bye;
            memset(&bye, 0, sizeof bye);
            bye.req_id = req.req_id;
            ipc_send_response(req.client_pid, &bye);
            break;
        }

        response_t resp;
        memset(&resp, 0, sizeof resp);
        resp.req_id = req.req_id;
        resp.status = ST_OK;
        resp.fd = req.fd;
        uint32_t n = req.length;
        if (n > sizeof req.data) n = sizeof req.data;
        memcpy(resp.data, req.data, n);
        resp.length = n;

        if (ipc_send_response(req.client_pid, &resp) < 0)
            fprintf(stderr, "[echo_server] could not reply to pid %d\n", (int)req.client_pid);
        served++;
    }
    printf("[echo_server] served %lu requests, exiting\n", served);
    ipc_server_close(sfd);
    return 0;
}
