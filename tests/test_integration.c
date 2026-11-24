#include <stdio.h>
#include <semaphore.h>

#include "sched.h"

extern int mock_read_count;
extern int mock_write_count;

static int cache_lookup(int block)
{
    static int cache_line = -1;
    if (cache_line == block) {
        return 1;
    }
    cache_line = block;
    return 0;
}

static int cache_miss_submit(int block)
{
    struct disk_req req;

    if (cache_lookup(block)) {
        return 0;
    }

    req.block = block;
    req.is_write = 0;
    req.buf = NULL;
    req.status = -1;
    if (sem_init(&req.done, 0, 0) != 0) {
        return -1;
    }

    if (sched_submit(&req) != 0) {
        return -1;
    }
    sem_destroy(&req.done);
    return req.status;
}

int main(void)
{
    struct sched_stats stats;

    mock_read_count = 0;
    mock_write_count = 0;

    if (sched_init() != 0) {
        fprintf(stderr, "scheduler initialization failed\n");
        return 1;
    }

    sched_set_head(53);
    sched_set_policy(SCHED_FCFS);

    if (cache_miss_submit(98) != 0) {
        fprintf(stderr, "cache miss did not reach scheduler\n");
        sched_shutdown();
        return 1;
    }

    sched_stats(&stats);
    if (stats.requests_served != 1 || stats.total_head_movement != 45) {
        fprintf(stderr, "integration counters incorrect: served=%d movement=%ld\n",
                stats.requests_served, stats.total_head_movement);
        sched_shutdown();
        return 1;
    }

    if (mock_read_count != 1) {
        fprintf(stderr, "cache miss did not issue one mock read\n");
        sched_shutdown();
        return 1;
    }

    sched_shutdown();
    puts("cache -> scheduler -> disk integration passed");
    return 0;
}
