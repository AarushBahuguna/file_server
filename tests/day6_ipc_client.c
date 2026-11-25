/* Day 6 IPC probe client: expects either a valid reply or a failed call. */
#include <stdio.h>
#include <string.h>
#include "ipc.h"

int main(int argc, char **argv) {
    int expect_success = argc > 1 && strcmp(argv[1], "success") == 0;
    if (ipc_client_init() < 0) {
        perror("ipc_client_init");
        return 2;
    }

    /* Keep fault-injection tests fast and deterministic. */
    ipc_set_timeout(1000);

    request_t req;
    response_t resp;
    memset(&req, 0, sizeof req);
    req.req_id = 0xD6000001u;
    req.cmd = CMD_WRITE;
    snprintf(req.user, sizeof req.user, "day6-test");
    snprintf(req.data, sizeof req.data, "day6-ipc-reliability-probe");
    req.length = (uint32_t)strlen(req.data);

    int rc = ipc_call(&req, &resp);
    ipc_client_close();

    if (expect_success) {
        if (rc < 0 || resp.req_id != req.req_id || resp.status != ST_OK ||
            resp.length != req.length || memcmp(resp.data, req.data, req.length) != 0) {
            fprintf(stderr, "FAIL: expected a matching successful response\n");
            return 1;
        }
        puts("PASS: matching response accepted");
        return 0;
    }

    if (rc >= 0) {
        fprintf(stderr, "FAIL: expected ipc_call to reject/timeout, got success\n");
        return 1;
    }
    puts("PASS: invalid/unavailable response rejected or timed out");
    return 0;
}
