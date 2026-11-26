#include "stats.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ================= server-side builders ================= */
static int put(char *buf, size_t cap, size_t *len, const char *key, const char *val) {
    int w = snprintf(buf + *len, cap - *len, "%s %s\n", key, val);
    if (w < 0 || (size_t)w >= cap - *len) { buf[*len] = '\0'; return -1; }
    *len += (size_t)w;
    return 0;
}

int stats_put_u64(char *buf, size_t cap, size_t *len, const char *key, unsigned long long v) {
    char t[32];
    snprintf(t, sizeof t, "%llu", v);
    return put(buf, cap, len, key, t);
}

int stats_put_dbl(char *buf, size_t cap, size_t *len, const char *key, double v) {
    char t[32];
    snprintf(t, sizeof t, "%.1f", v);
    return put(buf, cap, len, key, t);
}

int stats_put_str(char *buf, size_t cap, size_t *len, const char *key, const char *v) {
    return put(buf, cap, len, key, v);
}

/* ================= parsing ================= */
static int valid_key(const char *k) {
    if (!*k || !isalnum((unsigned char)k[0]) || strlen(k) >= STATS_KEY_LEN) return 0;
    int dot = 0;
    for (const char *p = k; *p; p++) {
        if (*p == '.') dot = 1;
        else if (!(islower((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '_')) return 0;
    }
    return dot;
}

int stats_parse(const char *text, size_t len, stats_t *out) {
    out->n = 0;
    char *copy = malloc(len + 1);
    if (!copy) return -1;
    memcpy(copy, text, len);
    copy[len] = '\0';

    int rc = 0;
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        while (isspace((unsigned char)*line)) line++;
        if (!*line || *line == '#') continue;
        char *sp = line;
        while (*sp && !isspace((unsigned char)*sp)) sp++;
        if (!*sp) { rc = -1; break; }                  /* key without a value */
        *sp++ = '\0';
        while (isspace((unsigned char)*sp)) sp++;
        size_t vl = strlen(sp);
        while (vl > 0 && isspace((unsigned char)sp[vl - 1])) sp[--vl] = '\0';
        if (!valid_key(line) || !*sp || vl >= STATS_VAL_LEN) { rc = -1; break; }   /* free text, not our format */
        if (out->n < STATS_MAX_ENTRIES) {
            snprintf(out->e[out->n].key, STATS_KEY_LEN, "%s", line);
            snprintf(out->e[out->n].val, STATS_VAL_LEN, "%s", sp);
            out->n++;
        }
    }
    free(copy);
    if (rc < 0) { out->n = 0; return -1; }
    return out->n;
}

/* ================= rendering ================= */
typedef struct { const char *key, *section, *label; } def_t;

static const def_t DEFS[] = {
    { "disk.total_blocks",   "Disk", "Total blocks" },
    { "disk.block_size",     "Disk", "Block size (bytes)" },
    { "disk.used_blocks",    "Disk", "Used blocks" },
    { "disk.free_blocks",    "Disk", "Free blocks" },
    { "disk.files",          "Disk", "Files" },
    { "disk.dirs",           "Disk", "Directories" },
    { "frag.free_extents",   "Fragmentation", "Free extents" },
    { "frag.largest_free",   "Fragmentation", "Largest free extent (blocks)" },
    { "frag.external_pct",   "Fragmentation", "External fragmentation (%)" },
    { "frag.internal_bytes", "Fragmentation", "Internal fragmentation (bytes)" },
    { "cache.capacity",      "Cache", "Capacity (blocks)" },
    { "cache.hits",          "Cache", "Hits" },
    { "cache.misses",        "Cache", "Misses" },
    { "cache.evictions",     "Cache", "Evictions" },
    { "cache.writebacks",    "Cache", "Write-backs (dirty flushes)" },
    { "sched.policy",        "Disk scheduler", "Policy" },
    { "sched.requests",      "Disk scheduler", "Requests served" },
    { "sched.head_movement", "Disk scheduler", "Total head movement (tracks)" },
    { "lock.acquired",       "Locks and deadlocks", "Locks acquired" },
    { "lock.waits",          "Locks and deadlocks", "Lock waits" },
    { "lock.max_wait_ms",    "Locks and deadlocks", "Longest lock wait (ms)" },
    { "deadlock.detected",   "Locks and deadlocks", "Deadlocks detected" },
    { "deadlock.victims",    "Locks and deadlocks", "Victims rolled back" },
    { "server.uptime_s",     "Server", "Uptime (s)" },
    { "server.requests",     "Server", "Requests handled" },
    { "sessions.active",     "Server", "Active sessions" },
};
#define NDEFS (sizeof DEFS / sizeof DEFS[0])

static const char *SECTIONS[] = { "Disk", "Fragmentation", "Cache", "Disk scheduler",
                                  "Locks and deadlocks", "Server" };
#define NSECTIONS (sizeof SECTIONS / sizeof SECTIONS[0])

static const char *get(const stats_t *s, const char *key) {
    for (int i = 0; i < s->n; i++) if (!strcmp(s->e[i].key, key)) return s->e[i].val;
    return NULL;
}

static int getd(const stats_t *s, const char *key, double *out) {
    const char *v = get(s, key);
    if (!v) return 0;
    char *end;
    *out = strtod(v, &end);
    return *end == '\0' && end != v;
}

static void row(FILE *fp, const char *label, const char *val) {
    fprintf(fp, "  %-34s %12s\n", label, val);
}

static void derived_rows(const stats_t *s, const char *section, FILE *fp) {
    double a, b, c;
    char t[32];
    if (!strcmp(section, "Disk")) {
        if (getd(s, "disk.total_blocks", &a) && a > 0) {
            if (getd(s, "disk.used_blocks", &b)) ;
            else if (getd(s, "disk.free_blocks", &c)) b = a - c;
            else return;
            snprintf(t, sizeof t, "%.1f", b * 100.0 / a);
            row(fp, "Usage (%)", t);
        }
    } else if (!strcmp(section, "Cache")) {
        if (getd(s, "cache.hits", &a) && getd(s, "cache.misses", &b) && a + b > 0) {
            snprintf(t, sizeof t, "%.1f", a * 100.0 / (a + b));
            row(fp, "Hit ratio (%)", t);
        }
    } else if (!strcmp(section, "Disk scheduler")) {
        if (getd(s, "sched.requests", &a) && a > 0 && getd(s, "sched.head_movement", &b)) {
            snprintf(t, sizeof t, "%.1f", b / a);
            row(fp, "Avg head movement per request", t);
        }
    }
}

static int is_known(const char *key) {
    for (size_t i = 0; i < NDEFS; i++) if (!strcmp(DEFS[i].key, key)) return 1;
    return 0;
}

static void render_table(const stats_t *s, FILE *fp) {
    int first = 1;
    for (size_t k = 0; k <= NSECTIONS; k++) {
        char *buf = NULL;
        size_t sz = 0;
        FILE *mem = open_memstream(&buf, &sz);
        if (!mem) return;
        const char *name = k < NSECTIONS ? SECTIONS[k] : "Other";
        if (k < NSECTIONS) {
            for (size_t i = 0; i < NDEFS; i++) {
                if (strcmp(DEFS[i].section, name)) continue;
                const char *v = get(s, DEFS[i].key);
                if (v) row(mem, DEFS[i].label, v);
            }
            derived_rows(s, name, mem);
        } else {
            for (int i = 0; i < s->n; i++)
                if (!is_known(s->e[i].key)) row(mem, s->e[i].key, s->e[i].val);
        }
        fclose(mem);
        if (sz > 0) {
            if (!first) fputc('\n', fp);
            fprintf(fp, "%s\n%s", name, buf);
            first = 0;
        }
        free(buf);
    }
    if (first) fputs("(no statistics reported)\n", fp);
}

void stats_render(const stats_t *s, stats_fmt_t fmt, FILE *fp) {
    switch (fmt) {
    case STATS_RAW:
        for (int i = 0; i < s->n; i++) fprintf(fp, "%s %s\n", s->e[i].key, s->e[i].val);
        break;
    case STATS_CSV:
        fputs("key,value\n", fp);
        for (int i = 0; i < s->n; i++) fprintf(fp, "%s,%s\n", s->e[i].key, s->e[i].val);
        break;
    default:
        render_table(s, fp);
    }
}
