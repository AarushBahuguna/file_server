/* stats.h - statistics text format + pretty printer (OWNER: Shubham)
 *
 * The server answers CMD_STATS with plain text, one "key value" pair per line. Every module owner
 * appends its own numbers with the stats_put_* helpers; the CLI turns them into a readable report
 * (table), raw text, or CSV (for graphs in the report).  Keep every key one of these (extra keys
 * are still shown, under "Other"):
 *
 *   Arjun    disk.total_blocks disk.block_size disk.used_blocks disk.free_blocks disk.files disk.dirs
 *            frag.free_extents frag.largest_free frag.external_pct frag.internal_bytes
 *   Shwetank cache.capacity cache.hits cache.misses cache.evictions cache.writebacks
 *            lock.acquired lock.waits lock.max_wait_ms server.uptime_s server.requests sessions.active
 *   Aarush   sched.policy sched.requests sched.head_movement deadlock.detected deadlock.victims
 *
 * Example server code (rs is the response_t being filled):
 *     size_t len = 0;
 *     stats_put_u64(rs->data, sizeof rs->data, &len, "cache.hits", hits);
 *     stats_put_str(rs->data, sizeof rs->data, &len, "sched.policy", "SCAN");
 *     rs->length = len;
 */
#ifndef STATS_H
#define STATS_H
#include <stdio.h>
#include <stddef.h>

#define STATS_MAX_ENTRIES 64
#define STATS_KEY_LEN 40
#define STATS_VAL_LEN 40

typedef enum { STATS_TABLE = 0, STATS_RAW = 1, STATS_CSV = 2 } stats_fmt_t;

typedef struct {
    int n;
    struct { char key[STATS_KEY_LEN]; char val[STATS_VAL_LEN]; } e[STATS_MAX_ENTRIES];
} stats_t;

/* ---- server side: append "key value\n" at buf+*len. Return 0, or -1 if it does not fit. ---- */
int stats_put_u64(char *buf, size_t cap, size_t *len, const char *key, unsigned long long v);
int stats_put_dbl(char *buf, size_t cap, size_t *len, const char *key, double v);   /* one decimal */
int stats_put_str(char *buf, size_t cap, size_t *len, const char *key, const char *v);

/* ---- client side ---- */
/* Returns number of entries, or -1 if the text is not in key/value format (old free-text answer). */
int  stats_parse(const char *text, size_t len, stats_t *out);
void stats_render(const stats_t *s, stats_fmt_t fmt, FILE *fp);

#endif
