/* Day 6 IPC fault-injection server. Uses the real IPC API.
 * modes:  normal   answers one request correctly
 *         wrong-id answers one request with the wrong req_id
 *         delay    answers one request after 3 s (the probe's timeout is 1 s)
 *         stale    answers request 1 after 2 s (too late), then answers request 2 at once */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ipc.h"

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "normal";
    int fd = ipc_server_init();
    if (fd < 0) {
        perror("ipc_server_init");
        return 1;
    }

    int total = strcmp(mode, "stale") == 0 ? 2 : 1;
    int failed = 0;
    for (int i = 0; i < total; i++) {
        request_t req;
        int rc = ipc_recv_request(fd, &req);
        if (rc <= 0) {
            fprintf(stderr, "server: ipc_recv_request failed (%d)\n", rc);
            ipc_server_close(fd);
            return 1;
        }

        if (strcmp(mode, "delay") == 0) sleep(3);
        if (strcmp(mode, "stale") == 0 && i == 0) sleep(2);

        response_t resp;
        memset(&resp, 0, sizeof resp);
        resp.req_id = req.req_id;
        if (strcmp(mode, "wrong-id") == 0) resp.req_id++;
        resp.status = ST_OK;
        resp.length = req.length;
        if (resp.length > sizeof resp.data) resp.length = sizeof resp.data;
        memcpy(resp.data, req.data, resp.length);

        if (ipc_send_response(req.client_pid, &resp) < 0) {
            perror("ipc_send_response");     /* expected in 'delay': the client already gave up */
            failed = 1;
        }
    }
    ipc_server_close(fd);
    return failed;
}
