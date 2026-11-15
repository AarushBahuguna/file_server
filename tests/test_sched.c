#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sched.h"

extern int mock_last_block;
extern int mock_last_op;
extern int mock_read_count;
extern int mock_write_count;

static int check_fcfs(int *blocks, int count)
{
    int i;
    int last = 0;
    long expected_movement = 0;

    for (i = 0; i < count; i++) {
        expected_movement += abs(last - blocks[i]);
        last = blocks[i];
    }

    return (int)expected_movement;
}

int main(void)
{
    const int count = 20;
    struct disk_req reqs[count];
    struct sched_stats stats;
    int blocks[count];
    int i;
    long expected_total = 0;

    sched_init();

    for (i = 0; i < count; i++) {
        blocks[i] = (i * 17) % 200;
        reqs[i].block = blocks[i];
        reqs[i].is_write = (i % 2 == 0);
        reqs[i].buf = NULL;
        reqs[i].status = -1;
        sem_init(&reqs[i].done, 0, 0);

        if (sched_submit(&reqs[i]) != 0) {
            fprintf(stderr, "sched_submit failed for request %d\n", i);
            return 1;
        }

        if (reqs[i].status != 0) {
            fprintf(stderr, "request %d completed with status %d\n", i, reqs[i].status);
            return 1;
        }
    }

    sched_stats(&stats);
    expected_total = check_fcfs(blocks, count);

    if (stats.requests_served != count) {
        fprintf(stderr, "expected %d requests served, got %d\n", count, stats.requests_served);
        return 1;
    }

    if (stats.total_head_movement != expected_total) {
        fprintf(stderr, "expected head movement %ld, got %ld\n", expected_total, stats.total_head_movement);
        return 1;
    }

    if (mock_read_count + mock_write_count != count) {
        fprintf(stderr, "expected %d disk operations, got %d\n", count, mock_read_count + mock_write_count);
        return 1;
    }

    sched_shutdown();
    puts("scheduler fcfs test passed");
    return 0;
}
