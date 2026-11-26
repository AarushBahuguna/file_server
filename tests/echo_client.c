/* echo_client.c - Day 2 test client (Shubham).
 * usage: echo_client <name> <count>   sends <count> echo requests and verifies replies
 *        echo_client --shutdown       stops echo_server */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipc.h"

int main(int argc, char **argv) {
    if (ipc_client_init() < 0) return 1;
    request_t req;  response_t resp;
    memset(&req, 0, sizeof req);

    if (argc >= 2 && strcmp(argv[1], "--shutdown") == 0) {
        req.cmd = CMD_LOGOUT;
        strcpy(req.path, "__shutdown__");
        ipc_call(&req, &resp);
        ipc_client_close();
        return 0;
    }

    const char *name = argc > 1 ? argv[1] : "client";
    int count = argc > 2 ? atoi(argv[2]) : 10;
    int bad = 0;

    for (int i = 0; i < count; i++) {
        memset(&req, 0, sizeof req);
        req.req_id = (uint32_t)i;
        req.cmd = CMD_WRITE;
        snprintf(req.user, sizeof req.user, "%s", name);
        int len = snprintf(req.data, sizeof req.data, "hello from %s #%d", name, i);
        req.length = (uint32_t)len;

        if (ipc_call(&req, &resp) < 0) { fprintf(stderr, "%s: ipc_call failed at #%d\n", name, i); bad++; break; }
        if (resp.req_id != req.req_id || resp.status != ST_OK ||
            resp.length != req.length || memcmp(resp.data, req.data, req.length) != 0) {
            fprintf(stderr, "%s: MISMATCH at #%d\n", name, i);
            bad++;
        }
    }
    ipc_client_close();
    printf("%s: %s (%d requests, %d bad)\n", name, bad ? "FAIL" : "PASS", count, bad);
    return bad ? 1 : 0;
}
