/* Day 6 IPC fault-injection server. Uses the existing Shubham IPC API. */
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

    request_t req;
    int rc = ipc_recv_request(fd, &req);
    if (rc <= 0) {
        fprintf(stderr, "server: ipc_recv_request failed (%d)\n", rc);
        ipc_server_close(fd);
        return 1;
    }

    if (strcmp(mode, "delay") == 0)
        sleep(3); /* Exceeds the 1-second timeout configured by the probe client. */

    response_t resp;
    memset(&resp, 0, sizeof resp);
    resp.req_id = req.req_id;
    if (strcmp(mode, "wrong-id") == 0)
        resp.req_id++;
    resp.status = ST_OK;
    resp.length = req.length;
    if (resp.length > sizeof resp.data)
        resp.length = sizeof resp.data;
    memcpy(resp.data, req.data, resp.length);

    if (ipc_send_response(req.client_pid, &resp) < 0) {
        perror("ipc_send_response");
        ipc_server_close(fd);
        return 1;
    }

    ipc_server_close(fd);
    return 0;
}
