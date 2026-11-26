/* Day 6 IPC probe client.
 *   day6_ipc_client success                 expects a matching reply
 *   day6_ipc_client failure [ENOENT|ETIMEDOUT]   expects ipc_call to fail (with that errno, if given)
 *   day6_ipc_client stale                   1st call must time out, 2nd call must get ITS OWN reply
 *                                           (the late reply to call 1 has to be skipped) */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "ipc.h"

static int errno_from_name(const char *n) {
    if (!strcmp(n, "ENOENT")) return ENOENT;
    if (!strcmp(n, "ETIMEDOUT")) return ETIMEDOUT;
    if (!strcmp(n, "ENXIO")) return ENXIO;
    return -1;
}

static void make_req(request_t *req, uint32_t id, const char *text) {
    memset(req, 0, sizeof *req);
    req->req_id = id;
    req->cmd = CMD_WRITE;
    snprintf(req->user, sizeof req->user, "day6-test");
    snprintf(req->data, sizeof req->data, "%s", text);
    req->length = (uint32_t)strlen(req->data);
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "failure";
    if (ipc_client_init() < 0) {
        perror("ipc_client_init");
        return 2;
    }
    ipc_set_timeout(1000);                       /* keep the tests fast */

    request_t req;
    response_t resp;

    if (!strcmp(mode, "stale")) {
        make_req(&req, 0xD6000001u, "first-request");
        int rc = ipc_call(&req, &resp);
        int e = errno;
        if (rc >= 0 || e != ETIMEDOUT) {
            fprintf(stderr, "FAIL: call 1 should time out (rc=%d errno=%d)\n", rc, e);
            ipc_client_close();
            return 1;
        }
        ipc_set_timeout(6000);                   /* long enough for the late reply AND the real one */
        make_req(&req, 0xD6000002u, "second-request");
        rc = ipc_call(&req, &resp);
        ipc_client_close();
        if (rc < 0 || resp.req_id != req.req_id || resp.length != req.length ||
            memcmp(resp.data, req.data, req.length) != 0) {
            fprintf(stderr, "FAIL: call 2 did not get its own reply (stale reply mistaken for it?)\n");
            return 1;
        }
        puts("PASS: late reply skipped, next call got its own reply");
        return 0;
    }

    make_req(&req, 0xD6000001u, "day6-ipc-reliability-probe");
    int rc = ipc_call(&req, &resp);
    int e = errno;
    ipc_client_close();

    if (!strcmp(mode, "success")) {
        if (rc < 0 || resp.req_id != req.req_id || resp.status != ST_OK ||
            resp.length != req.length || memcmp(resp.data, req.data, req.length) != 0) {
            fprintf(stderr, "FAIL: expected a matching successful reply\n");
            return 1;
        }
        puts("PASS: matching reply accepted");
        return 0;
    }

    if (rc >= 0) {
        fprintf(stderr, "FAIL: expected ipc_call to fail, but it succeeded\n");
        return 1;
    }
    if (argc > 2 && errno_from_name(argv[2]) != e) {
        fprintf(stderr, "FAIL: expected errno %s but got %d (%s)\n", argv[2], e, strerror(e));
        return 1;
    }
    printf("PASS: ipc_call failed as expected (errno=%d %s)\n", e, strerror(e));
    return 0;
}
