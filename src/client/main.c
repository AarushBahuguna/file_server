/* CLI client (OWNER: Shubham).  usage: client [username]
 * Env: FS_CLIENT_LOG=<file> enables request logging, FS_TIMEOUT_MS=<ms> sets the reply timeout.
 * Reads commands from the terminal or from a pipe/script on stdin. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include "ipc.h"
#include "parse.h"
#include "log.h"
#include "stats.h"

#define MAX_TRACKED 64

static uint32_t g_req_id = 1;
static char g_user[PROTO_MAX_USER] = "guest";
static char g_cwd[PROTO_MAX_PATH]  = "/";
static int  g_dead = 0;                       /* server unreachable */
static int  g_timeout_ms = 30000;
static volatile sig_atomic_t g_stop = 0;

/* fds this client has opened; closed automatically on exit so no locks are left behind */
static int g_fds[MAX_TRACKED];
static int g_nfds = 0;

static void on_signal(int s) { (void)s; g_stop = 1; }

static int track_has(int fd) { for (int i = 0; i < g_nfds; i++) if (g_fds[i] == fd) return 1; return 0; }
static void track_add(int fd) { if (!track_has(fd) && g_nfds < MAX_TRACKED) g_fds[g_nfds++] = fd; }
static void track_del(int fd) {
    for (int i = 0; i < g_nfds; i++) if (g_fds[i] == fd) { g_fds[i] = g_fds[--g_nfds]; return; }
}

static int send_req(request_t *req, response_t *resp) {
    memset(resp, 0, sizeof *resp);
    req->req_id = g_req_id++;
    snprintf(req->user, sizeof req->user, "%s", g_user);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int rc = ipc_call(req, resp);
    int e = errno;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (double)(t1.tv_sec - t0.tv_sec) * 1000.0 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;

    if (rc < 0) {
        log_request(req, NULL, ms);
        if (e == EINTR) g_stop = 1;
        else if (e == ETIMEDOUT)
            fprintf(stderr, "error: no reply from the server after %d s (server busy, or waiting on a file lock held by another user)\n",
                    g_timeout_ms / 1000);
        else if (e == ENOENT || e == ENXIO) {
            fprintf(stderr, "error: cannot reach the server. Is it running? (start it with ./bin/server)\n");
            g_dead = 1;
        } else {
            fprintf(stderr, "error: lost connection to the server (%s)\n", strerror(e));
            g_dead = 1;
        }
        return -1;
    }
    log_request(req, resp, ms);
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

static void print_error(const parsed_cmd_t *c, int st) {
    char verb[16];
    const char *nm = cmd_name(c->cmd);
    size_t i;
    for (i = 0; nm[i] && i < sizeof verb - 1; i++) verb[i] = (char)tolower((unsigned char)nm[i]);
    verb[i] = '\0';

    char target[PROTO_MAX_PATH + 16] = "";
    switch (c->cmd) {
    case CMD_READ: case CMD_WRITE: case CMD_CLOSE: snprintf(target, sizeof target, " fd %d", c->fd); break;
    case CMD_STATS: case CMD_LOGIN: case CMD_LOGOUT: break;
    default: if (c->path[0]) snprintf(target, sizeof target, " %s", c->path);
    }
    fprintf(stderr, "error: %s%s: %s\n", verb, target, status_str(st));

    switch (st) {
    case ST_ERR_DEADLOCK: fprintf(stderr, "  your request was the deadlock victim and was rolled back; just run it again\n"); break;
    case ST_ERR_NOSPC:    fprintf(stderr, "  the disk is full; delete something or check 'stats'\n"); break;
    case ST_ERR_BUSY:     fprintf(stderr, "  another user holds a conflicting lock; try again in a moment\n"); break;
    case ST_ERR_BADFD:    fprintf(stderr, "  use 'open <path>' first to get a valid fd\n"); break;
    case ST_ERR_PERM:     fprintf(stderr, "  not allowed (are you logged in? did you open the file with 'w' before writing?)\n"); break;
    case ST_ERR_EXIST:    fprintf(stderr, "  pick another name, or 'delete' the old one first\n"); break;
    default: break;
    }
}

static void print_text(const response_t *rs) {
    uint32_t n = rs->length > PROTO_MAX_RESP_DATA ? PROTO_MAX_RESP_DATA : rs->length;
    if (n) {
        fwrite(rs->data, 1, n, stdout);
        if (rs->data[n - 1] != '\n') putchar('\n');
    }
}

/* Server sends "key value" lines; show them as a report. Old free-text answers are printed as they are. */
static void print_stats(const response_t *rs, int fmt) {
    char text[PROTO_MAX_RESP_DATA + 1];
    uint32_t n = rs->length > PROTO_MAX_RESP_DATA ? PROTO_MAX_RESP_DATA : rs->length;
    memcpy(text, rs->data, n);
    text[n] = '\0';
    stats_t st;
    if (stats_parse(text, n, &st) < 0) { print_text(rs); return; }
    stats_render(&st, (stats_fmt_t)fmt, stdout);
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
        if (rs.status != ST_OK) { print_error(c, rs.status); return; }
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
        if (rs.status != ST_OK) { print_error(c, rs.status); return; }
        size_t w = rs.length;
        if (w == 0) { fprintf(stderr, "error: write fd %d: short write (0 bytes accepted)\n", c->fd); return; }
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
    if (rs.status != ST_OK) { print_error(c, rs.status); return; }
    switch (c->cmd) {
    case CMD_LS:
        print_text(&rs);
        break;
    case CMD_STATS:
        print_stats(&rs, c->stats_fmt);
        break;
    case CMD_CD:
        if (rs.length > 0) {                      /* server returns the new working directory */
            size_t n = rs.length < sizeof g_cwd - 1 ? rs.length : sizeof g_cwd - 1;
            memcpy(g_cwd, rs.data, n);
            g_cwd[n] = '\0';
        }
        puts(g_cwd);
        break;
    case CMD_OPEN:
        track_add(rs.fd);
        printf("fd = %d\n", rs.fd);
        break;
    case CMD_CLOSE:
        track_del(c->fd);
        puts("ok");
        break;
    default:
        puts("ok");
    }
}

static void quiet_call(cmd_t cmd, int fd) {
    parsed_cmd_t pc = { .cmd = cmd, .fd = fd };
    request_t rq; response_t rs;
    base_req(&rq, &pc);
    rq.req_id = g_req_id++;
    snprintf(rq.user, sizeof rq.user, "%s", g_user);
    if (ipc_call(&rq, &rs) == 0) log_request(&rq, &rs, 0.0);
}

int main(int argc, char **argv) {
    const char *u = argc > 1 ? argv[1] : getenv("USER");
    if (u && *u) snprintf(g_user, sizeof g_user, "%s", u);
    if (getenv("FS_CLIENT_LOG")) log_init(getenv("FS_CLIENT_LOG"), LOG_INFO, 0);
    if (getenv("FS_TIMEOUT_MS")) g_timeout_ms = atoi(getenv("FS_TIMEOUT_MS"));

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;                    /* no SA_RESTART: Ctrl-C interrupts fgets/poll */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (ipc_client_init() < 0) return 1;
    ipc_set_timeout(g_timeout_ms);

    parsed_cmd_t login = { .cmd = CMD_LOGIN };
    request_t rq; response_t rs;
    base_req(&rq, &login);
    if (send_req(&rq, &rs) < 0) { ipc_client_close(); return 1; }
    if (rs.status != ST_OK) {
        fprintf(stderr, "login failed: %s\n", status_str(rs.status));
        ipc_client_close();
        return 1;
    }
    log_write(LOG_INFO, "client", "user %s logged in", g_user);

    int tty = isatty(STDIN_FILENO);
    if (tty) printf("Logged in as %s. Type 'help' for commands.\n", g_user);

    char line[PARSE_MAX_TEXT + 256];
    while (!g_dead && !g_stop) {
        if (tty) { printf("%s:%s> ", g_user, g_cwd); fflush(stdout); }
        if (!fgets(line, sizeof line, stdin)) break;

        parsed_cmd_t pc;
        char err[160];
        parse_result_t r = parse_command(line, &pc, err, sizeof err);
        if (r == P_EMPTY) continue;
        if (r == P_ERROR) { fprintf(stderr, "error: %s\n", err); continue; }
        if (r == P_HELP)  { print_help(); continue; }
        if (r == P_EXIT)  break;

        if ((pc.cmd == CMD_READ || pc.cmd == CMD_WRITE || pc.cmd == CMD_CLOSE) && !track_has(pc.fd)) {
            fprintf(stderr, "error: fd %d is not an open fd (use 'open <path>' first)\n", pc.fd);
            continue;
        }

        if (pc.cmd == CMD_READ) do_read(&pc);
        else if (pc.cmd == CMD_WRITE) do_write(&pc);
        else do_simple(&pc);
        fflush(stdout);
    }
    if (g_stop && tty) putchar('\n');

    if (!g_dead) {                                /* release everything this session still holds */
        g_stop = 0;
        while (g_nfds > 0) { int fd = g_fds[g_nfds - 1]; quiet_call(CMD_CLOSE, fd); g_nfds--; }
        quiet_call(CMD_LOGOUT, 0);
    }
    log_write(LOG_INFO, "client", "user %s logged out", g_user);
    log_close();
    ipc_client_close();
    return 0;
}
