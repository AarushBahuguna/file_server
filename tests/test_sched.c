#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sched.h"

extern int mock_last_block;
extern int mock_last_op;
extern int mock_read_count;
extern int mock_write_count;
extern void mock_reset_blocks(void);
extern int mock_block_read_count(int block);
extern int mock_block_write_count(int block);

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

static int test_random_workload(int policy)
{
    enum { RANDOM_COUNT = 200 };
    struct disk_req reqs[RANDOM_COUNT];
    struct sched_stats stats;
    int i;

    mock_read_count = 0;
    mock_write_count = 0;
    mock_reset_blocks();

    sched_init();
    sched_set_head(53);
    sched_set_policy(policy);
    srand(0xD3A6 + policy);

    for (i = 0; i < RANDOM_COUNT; i++) {
        reqs[i].block = rand() % 200;
        reqs[i].is_write = (i % 3 == 0);
        reqs[i].buf = NULL;
        reqs[i].status = -1;
        sem_init(&reqs[i].done, 0, 0);

        if (sched_submit(&reqs[i]) != 0) {
            sched_shutdown();
            fprintf(stderr, "random request %d was lost\n", i);
            return 1;
        }

        if (reqs[i].status != 0) {
            sched_shutdown();
            fprintf(stderr, "random request %d completed incorrectly\n", i);
            return 1;
        }

        if (mock_block_read_count(reqs[i].block) + mock_block_write_count(reqs[i].block) < 1) {
            sched_shutdown();
            fprintf(stderr, "random request %d was not served by the mock disk\n", i);
            return 1;
        }
    }

    sched_stats(&stats);
    if (stats.requests_served != RANDOM_COUNT) {
        sched_shutdown();
        fprintf(stderr, "random workload served %d of %d requests\n",
                stats.requests_served, RANDOM_COUNT);
        return 1;
    }

    if (mock_read_count + mock_write_count != RANDOM_COUNT) {
        sched_shutdown();
        fprintf(stderr, "random workload issued %d of %d disk operations\n",
                mock_read_count + mock_write_count, RANDOM_COUNT);
        return 1;
    }

    sched_shutdown();
    return 0;
}

static long simulate_policy_total(int policy, int head, int *queue, int count, int direction)
{
    int remaining[32];
    int remaining_count = count;
    int i;
    int current_head = head;
    int current_direction = direction;
    long total_movement = 0;

    for (i = 0; i < count; i++) {
        remaining[i] = queue[i];
    }

    while (remaining_count > 0) {
        int next_block = -1;
        int has_candidate = 0;

        if (sched_pick_next(remaining, remaining_count, current_head, current_direction, policy, &next_block) != 0) {
            fprintf(stderr, "sched_pick_next failed for policy %d\n", policy);
            return -1;
        }

        if (policy == SCHED_CSCAN) {
            int j;
            int has_forward = 0;
            int has_backward = 0;
            for (j = 0; j < remaining_count; j++) {
                if (remaining[j] >= current_head) {
                    has_forward = 1;
                }
                if (remaining[j] <= current_head) {
                    has_backward = 1;
                }
            }

            if (current_direction > 0 && !has_forward) {
                total_movement += (199 - current_head) + 199;
                current_head = 0;
            } else if (current_direction < 0 && !has_backward) {
                total_movement += current_head + 199;
                current_head = 199;
            }
        } else if (policy == SCHED_SCAN) {
            int j;
            int old_direction = current_direction;
            for (j = 0; j < remaining_count; j++) {
                if ((current_direction > 0 && remaining[j] >= current_head) ||
                    (current_direction < 0 && remaining[j] <= current_head)) {
                    has_candidate = 1;
                    break;
                }
            }
            if (!has_candidate) {
                if (old_direction < 0) {
                    total_movement += current_head - 0;
                    current_head = 0;
                } else {
                    total_movement += 199 - current_head;
                    current_head = 199;
                }
                current_direction *= -1;
            }
        }

        total_movement += abs(current_head - next_block);
        current_head = next_block;

        for (i = 0; i < remaining_count; i++) {
            if (remaining[i] == next_block) {
                for (int j = i; j + 1 < remaining_count; j++) {
                    remaining[j] = remaining[j + 1];
                }
                remaining_count--;
                break;
            }
        }
    }

    return total_movement;
}

int main(void)
{
    const int count = 20;
    struct disk_req reqs[count];
    struct sched_stats stats;
    int blocks[count];
    int i;
    long expected_total = 0;
    int textbook[] = {98, 183, 37, 122, 14, 124, 65, 67};

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

    if (simulate_policy_total(SCHED_FCFS, 53, textbook, 8, 1) != 640) {
        fprintf(stderr, "FCFS textbook example failed\n");
        return 1;
    }
    puts("scheduler fcfs textbook case passed");

    if (simulate_policy_total(SCHED_SCAN, 53, textbook, 8, -1) != 236) {
        fprintf(stderr, "SCAN textbook example failed\n");
        return 1;
    }
    puts("scheduler scan textbook case passed");

    if (simulate_policy_total(SCHED_CSCAN, 53, textbook, 8, 1) != 382) {
        fprintf(stderr, "C-SCAN textbook example failed\n");
        return 1;
    }
    puts("scheduler cscan textbook case passed");

    if (test_random_workload(SCHED_FCFS) != 0) {
        return 1;
    }
    puts("scheduler fcfs random workload passed");

    if (test_random_workload(SCHED_SCAN) != 0) {
        return 1;
    }
    puts("scheduler scan random workload passed");

    if (test_random_workload(SCHED_CSCAN) != 0) {
        return 1;
    }
    puts("scheduler cscan random workload passed");
    return 0;
}
