#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "stats.h"

static char *render(const stats_t *s, stats_fmt_t f) {
    char *out = NULL;
    size_t sz = 0;
    FILE *fp = open_memstream(&out, &sz);
    stats_render(s, f, fp);
    fclose(fp);
    return out;
}

int main(void) {
    char buf[3072];
    size_t len = 0;
    assert(stats_put_u64(buf, sizeof buf, &len, "disk.total_blocks", 1024) == 0);
    assert(stats_put_u64(buf, sizeof buf, &len, "disk.used_blocks", 224) == 0);
    assert(stats_put_u64(buf, sizeof buf, &len, "cache.hits", 80) == 0);
    assert(stats_put_u64(buf, sizeof buf, &len, "cache.misses", 20) == 0);
    assert(stats_put_str(buf, sizeof buf, &len, "sched.policy", "SCAN") == 0);
    assert(stats_put_u64(buf, sizeof buf, &len, "sched.requests", 200) == 0);
    assert(stats_put_u64(buf, sizeof buf, &len, "sched.head_movement", 5400) == 0);
    assert(stats_put_dbl(buf, sizeof buf, &len, "frag.external_pct", 12.5) == 0);
    assert(stats_put_u64(buf, sizeof buf, &len, "custom.thing", 7) == 0);

    stats_t st;
    assert(stats_parse(buf, len, &st) == 9);

    char *t = render(&st, STATS_TABLE);
    assert(strstr(t, "Disk\n") && strstr(t, "Total blocks"));
    assert(strstr(t, "Usage (%)") && strstr(t, "21.9"));            /* 224/1024 */
    assert(strstr(t, "Hit ratio (%)") && strstr(t, "80.0"));        /* 80/(80+20) */
    assert(strstr(t, "Avg head movement per request") && strstr(t, "27.0"));   /* 5400/200 */
    assert(strstr(t, "External fragmentation (%)") && strstr(t, "12.5"));
    assert(strstr(t, "Other\n") && strstr(t, "custom.thing"));      /* unknown keys are kept */
    assert(strstr(t, "SCAN"));
    free(t);

    char *c = render(&st, STATS_CSV);
    assert(!strncmp(c, "key,value\n", 10) && strstr(c, "cache.hits,80\n"));
    free(c);
    char *r = render(&st, STATS_RAW);
    assert(strstr(r, "disk.total_blocks 1024\n"));
    free(r);

    /* derived rows are skipped when their inputs are missing */
    const char *few = "cache.hits 5\n";
    assert(stats_parse(few, strlen(few), &st) == 1);
    t = render(&st, STATS_TABLE);
    assert(strstr(t, "Hits") && !strstr(t, "Hit ratio"));
    free(t);

    /* blank lines and comments are fine; old free-text answers are rejected */
    const char *ok = "# comment\n\n  disk.files   3  \n";
    assert(stats_parse(ok, strlen(ok), &st) == 1 && !strcmp(st.e[0].val, "3"));
    const char *legacy = "dummy stats: nothing real yet\nsessions: 1 active\n";
    assert(stats_parse(legacy, strlen(legacy), &st) == -1);
    const char *nokey = "disk.files\n";
    assert(stats_parse(nokey, strlen(nokey), &st) == -1);

    /* nothing reported */
    assert(stats_parse("", 0, &st) == 0);
    t = render(&st, STATS_TABLE);
    assert(strstr(t, "no statistics"));
    free(t);

    /* builder reports overflow instead of truncating silently */
    char tiny[16];
    size_t tl = 0;
    assert(stats_put_u64(tiny, sizeof tiny, &tl, "disk.total_blocks", 1) == -1 && tl == 0);

    printf("test_stats: PASS\n");
    return 0;
}
