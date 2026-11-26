#include "parse.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#define FAIL(...) do { snprintf(err, errlen, __VA_ARGS__); return P_ERROR; } while (0)

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Copies the next token into buf. Returns pointer after it, or NULL if none. */
static const char *next_token(const char *p, char *buf, size_t n, int *too_long) {
    p = skip_ws(p);
    if (!*p) return NULL;
    size_t i = 0;
    *too_long = 0;
    while (*p && !isspace((unsigned char)*p)) {
        if (i + 1 < n) buf[i++] = *p; else *too_long = 1;
        p++;
    }
    buf[i] = '\0';
    return p;
}

static int to_uint(const char *s, uint32_t *out) {
    if (!*s || s[0] == '-' || s[0] == '+') return -1;
    char *end;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (*end || errno || v > 0x7fffffffUL) return -1;
    *out = (uint32_t)v;
    return 0;
}

static const struct { const char *name; cmd_t cmd; int need_path; const char *usage; } PATH_CMDS[] = {
    { "mkdir",  CMD_MKDIR,  1, "mkdir <path>"  },
    { "cd",     CMD_CD,     0, "cd [path]"     },
    { "ls",     CMD_LS,     0, "ls [path]"     },
    { "create", CMD_CREATE, 1, "create <path>" },
    { "touch",  CMD_CREATE, 1, "touch <path>"  },
    { "delete", CMD_DELETE, 1, "delete <path>" },
    { "rm",     CMD_DELETE, 1, "rm <path>"     },
};

parse_result_t parse_command(const char *line, parsed_cmd_t *out, char *err, size_t errlen) {
    memset(out, 0, sizeof *out);
    err[0] = '\0';
    char w[32];
    int tl = 0;
    const char *p = next_token(line, w, sizeof w, &tl);
    if (!p || w[0] == '#') return P_EMPTY;
    if (tl) FAIL("unknown command (type 'help')");

    if (!strcmp(w, "help") || !strcmp(w, "?")) return P_HELP;
    if (!strcmp(w, "exit") || !strcmp(w, "quit")) return P_EXIT;

    /* ---- commands that take an optional/required path ---- */
    for (size_t i = 0; i < sizeof PATH_CMDS / sizeof PATH_CMDS[0]; i++) {
        if (strcmp(w, PATH_CMDS[i].name)) continue;
        char tok[PROTO_MAX_PATH + 1];
        const char *q = next_token(p, tok, sizeof tok, &tl);
        if (!q) {
            if (PATH_CMDS[i].need_path) FAIL("usage: %s", PATH_CMDS[i].usage);
            if (PATH_CMDS[i].cmd == CMD_CD) strcpy(out->path, "/");
        } else {
            if (tl || strlen(tok) >= PROTO_MAX_PATH) FAIL("path too long (max %d characters)", PROTO_MAX_PATH - 1);
            strcpy(out->path, tok);
            p = q;
        }
        if (*skip_ws(p)) FAIL("too many arguments; usage: %s", PATH_CMDS[i].usage);
        out->cmd = PATH_CMDS[i].cmd;
        return P_SERVER;
    }

    if (!strcmp(w, "open")) {
        char tok[PROTO_MAX_PATH + 1], m[8];
        const char *q = next_token(p, tok, sizeof tok, &tl);
        if (!q) FAIL("usage: open <path> [r|w]");
        if (tl || strlen(tok) >= PROTO_MAX_PATH) FAIL("path too long (max %d characters)", PROTO_MAX_PATH - 1);
        strcpy(out->path, tok);
        p = q;
        out->mode = OPEN_READ;
        q = next_token(p, m, sizeof m, &tl);
        if (q) {
            if (!strcmp(m, "r") || !strcmp(m, "read")) out->mode = OPEN_READ;
            else if (!strcmp(m, "w") || !strcmp(m, "write")) out->mode = OPEN_WRITE;
            else FAIL("mode must be r or w");
            p = q;
        }
        if (*skip_ws(p)) FAIL("too many arguments; usage: open <path> [r|w]");
        out->cmd = CMD_OPEN;
        return P_SERVER;
    }

    if (!strcmp(w, "stats")) {
        char f[16];
        const char *q = next_token(p, f, sizeof f, &tl);
        if (q) {
            if (tl) FAIL("usage: stats [table|raw|csv]");
            if (!strcmp(f, "table")) out->stats_fmt = 0;
            else if (!strcmp(f, "raw")) out->stats_fmt = 1;
            else if (!strcmp(f, "csv")) out->stats_fmt = 2;
            else FAIL("usage: stats [table|raw|csv]");
            p = q;
        }
        if (*skip_ws(p)) FAIL("usage: stats [table|raw|csv]");
        out->cmd = CMD_STATS;
        return P_SERVER;
    }

    if (!strcmp(w, "close")) {
        char a[32]; uint32_t fd;
        const char *q = next_token(p, a, sizeof a, &tl);
        if (!q || tl || to_uint(a, &fd)) FAIL("usage: close <fd>");
        if (*skip_ws(q)) FAIL("too many arguments; usage: close <fd>");
        out->cmd = CMD_CLOSE;
        out->fd = (int)fd;
        return P_SERVER;
    }

    if (!strcmp(w, "read")) {
        char a[32], b[32], c[32]; uint32_t fd, off, len;
        const char *q = next_token(p, a, sizeof a, &tl);
        if (q) q = next_token(q, b, sizeof b, &tl);
        if (q) q = next_token(q, c, sizeof c, &tl);
        if (!q || to_uint(a, &fd) || to_uint(b, &off) || to_uint(c, &len))
            FAIL("usage: read <fd> <offset> <length>");
        if (*skip_ws(q)) FAIL("too many arguments; usage: read <fd> <offset> <length>");
        out->cmd = CMD_READ;
        out->fd = (int)fd; out->offset = off; out->length = len;
        return P_SERVER;
    }

    if (!strcmp(w, "write")) {
        char a[32], b[32]; uint32_t fd, off;
        const char *q = next_token(p, a, sizeof a, &tl);
        if (q) q = next_token(q, b, sizeof b, &tl);
        if (!q || to_uint(a, &fd) || to_uint(b, &off)) FAIL("usage: write <fd> <offset> <text...>");
        q = skip_ws(q);
        size_t n = strlen(q);
        while (n > 0 && isspace((unsigned char)q[n - 1])) n--;      /* trim trailing newline */
        if (n == 0) FAIL("usage: write <fd> <offset> <text...>");
        if (n >= PARSE_MAX_TEXT) FAIL("text too long (max %d characters)", PARSE_MAX_TEXT - 1);
        memcpy(out->text, q, n);
        out->text[n] = '\0';
        out->text_len = n;
        out->cmd = CMD_WRITE;
        out->fd = (int)fd; out->offset = off;
        return P_SERVER;
    }

    FAIL("unknown command '%s' (type 'help')", w);
}

void print_help(void) {
    puts("Commands:\n"
         "  mkdir <path>                   create a directory\n"
         "  cd [path]                      change directory (no path = /)\n"
         "  ls [path]                      list a directory\n"
         "  create <path>                  create an empty file   (alias: touch)\n"
         "  open <path> [r|w]              open a file, prints its fd (default r)\n"
         "  close <fd>                     close a file\n"
         "  read <fd> <offset> <length>    read bytes from an open file\n"
         "  write <fd> <offset> <text...>  write text to an open file\n"
         "  delete <path>                  delete a file             (alias: rm)\n"
         "  stats [table|raw|csv]          disk, fragmentation, cache, scheduler, locks (csv for graphs)\n"
         "  help                           this message\n"
         "  exit                           log out and quit");
}
