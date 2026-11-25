/* CLI client (OWNER: Shubham).  usage: client [username]
 * Reads commands from the terminal or from a pipe/script on stdin. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ipc.h"
#include "parse.h"

static uint32_t g_req_id = 1;
static char g_user[PROTO_MAX_USER] = "guest";
static char g_cwd[PROTO_MAX_PATH]  = "/";
static int  g_dead = 0;               /* server unreachable */

static int send_req(request_t *req, response_t *resp) {
    memset(resp, 0, sizeof *resp);
    req->req_id = g_req_id++;
    snprintf(req->user, sizeof req->user, "%s", g_user);
    if (ipc_call(req, resp) < 0) {
        fprintf(stderr, "error: lost connection to server\n");
        g_dead = 1;
        return -1;
    }
    return 0;
}

static void base_req(request_t *r, const parsed_cmd_t *c) {
    memset(r, 0, sizeof *r);
    r->cmd = c->cmd;
    r->fd = c->fd;
    r->mode = c->mode;
    r->offset = c->offset;
    r->length = c->length;
    memcpy(r->path, c->path, sizeof r->path);
}

static void print_text(const response_t *rs) {
    uint32_t n = rs->length > PROTO_MAX_RESP_DATA ? PROTO_MAX_RESP_DATA : rs->length;
    if (n) {
        fwrite(rs->data, 1, n, stdout);
        if (rs->data[n - 1] != '\n') putchar('\n');
    }
}

static void do_read(const parsed_cmd_t *c) {
    uint32_t remaining = c->length, off = c->offset, total = 0;
    while (remaining > 0) {
        uint32_t n = remaining > PROTO_MAX_RESP_DATA ? PROTO_MAX_RESP_DATA : remaining;
        request_t rq; response_t rs;
        base_req(&rq, c);
        rq.offset = off;
        rq.length = n;
        if (send_req(&rq, &rs) < 0) return;
        if (rs.status != ST_OK) { fprintf(stderr, "error: %s\n", status_str(rs.status)); return; }
        uint32_t got = rs.length > PROTO_MAX_RESP_DATA ? PROTO_MAX_RESP_DATA : rs.length;
        fwrite(rs.data, 1, got, stdout);
        total += got;
        if (got < n) break;                      /* end of file */
        off += got;
        remaining -= got;
    }
    if (total) putchar('\n'); else puts("(empty)");
}

static void do_write(const parsed_cmd_t *c) {
    size_t done = 0;
    uint32_t off = c->offset;
    while (done < c->text_len) {
        size_t n = c->text_len - done;
        if (n > PROTO_MAX_REQ_DATA) n = PROTO_MAX_REQ_DATA;
        request_t rq; response_t rs;
        base_req(&rq, c);
        rq.offset = off;
        rq.length = (uint32_t)n;
        memcpy(rq.data, c->text + done, n);
        if (send_req(&rq, &rs) < 0) return;
        if (rs.status != ST_OK) { fprintf(stderr, "error: %s\n", status_str(rs.status)); return; }
        size_t w = rs.length;
        if (w == 0) { fprintf(stderr, "error: short write\n"); return; }
        if (w > n) w = n;
        done += w;
        off += (uint32_t)w;
    }
    printf("wrote %zu bytes\n", done);
}

static void do_simple(const parsed_cmd_t *c) {
    request_t rq; response_t rs;
    base_req(&rq, c);
    if (send_req(&rq, &rs) < 0) return;
    if (rs.status != ST_OK) { fprintf(stderr, "error: %s\n", status_str(rs.status)); return; }
    switch (c->cmd) {
    case CMD_LS:
    case CMD_STATS:
        print_text(&rs);
        break;
    case CMD_CD:
        if (rs.length > 0) {                      /* server returns the new working directory */
            size_t n = rs.length < sizeof g_cwd - 1 ? rs.length : sizeof g_cwd - 1;
            memcpy(g_cwd, rs.data, n);
            g_cwd[n] = '\0';
        }
        puts("ok");
        break;
    case CMD_OPEN:
        printf("fd = %d\n", rs.fd);
        break;
    default:
        puts("ok");
    }
}

int main(int argc, char **argv) {
    const char *u = argc > 1 ? argv[1] : getenv("USER");
    if (u && *u) snprintf(g_user, sizeof g_user, "%s", u);

    if (ipc_client_init() < 0) return 1;

    parsed_cmd_t login = { .cmd = CMD_LOGIN };
    request_t rq; response_t rs;
    base_req(&rq, &login);
    if (send_req(&rq, &rs) < 0) { ipc_client_close(); return 1; }
    if (rs.status != ST_OK) {
        fprintf(stderr, "login failed: %s\n", status_str(rs.status));
        ipc_client_close();
        return 1;
    }

    int tty = isatty(STDIN_FILENO);
    if (tty) printf("Logged in as %s. Type 'help' for commands.\n", g_user);

    char line[PARSE_MAX_TEXT + 256];
    while (!g_dead) {
        if (tty) { printf("%s:%s> ", g_user, g_cwd); fflush(stdout); }
        if (!fgets(line, sizeof line, stdin)) break;

        parsed_cmd_t pc;
        char err[160];
        parse_result_t r = parse_command(line, &pc, err, sizeof err);
        if (r == P_EMPTY) continue;
        if (r == P_ERROR) { fprintf(stderr, "error: %s\n", err); continue; }
        if (r == P_HELP)  { print_help(); continue; }
        if (r == P_EXIT)  break;

        if (pc.cmd == CMD_READ) do_read(&pc);
        else if (pc.cmd == CMD_WRITE) do_write(&pc);
        else do_simple(&pc);
        fflush(stdout);
    }

    if (!g_dead) {                                /* best-effort logout */
        parsed_cmd_t lo = { .cmd = CMD_LOGOUT };
        base_req(&rq, &lo);
        send_req(&rq, &rs);
    }
    ipc_client_close();
    return 0;
}
