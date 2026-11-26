#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISK_MAX 199
#define REQUEST_COUNT 1000
#define CACHE_SIZE 64
#define RUNS 5

typedef struct {
    int block;
} request_t;

static int policy_movement(int policy, int start_head,
                           request_t *requests, int count)
{
    int remaining[REQUEST_COUNT];
    int remaining_count = count;
    int head = start_head;
    int direction = (policy == 1) ? -1 : 1;
    int movement = 0;
    int i;

    for (i = 0; i < count; i++) {
        remaining[i] = requests[i].block;
    }

    while (remaining_count > 0) {
        int best = -1;
        int best_index = -1;
        int best_distance = DISK_MAX * 2;

        if (policy == 0) {
            best_index = 0;
            best = remaining[0];
        } else {
            for (i = 0; i < remaining_count; i++) {
                int candidate = remaining[i];
                int in_direction = (direction > 0) ? candidate >= head
                                                     : candidate <= head;
                if (in_direction) {
                    int distance = abs(head - candidate);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = i;
                        best = candidate;
                    }
                }
            }

            if (best_index < 0) {
                if (policy == 1) {
                    if (direction > 0) {
                        movement += DISK_MAX - head;
                        head = DISK_MAX;
                    } else {
                        movement += head;
                        head = 0;
                    }
                    direction *= -1;
                } else {
                    if (direction > 0) {
                        movement += DISK_MAX - head + DISK_MAX;
                        head = 0;
                    } else {
                        movement += head + DISK_MAX;
                        head = DISK_MAX;
                    }
                }

                best_index = 0;
                if (policy == 2) {
                    if (direction > 0) {
                        best = remaining[0];
                        for (i = 1; i < remaining_count; i++) {
                            if (remaining[i] < best) best = remaining[i];
                        }
                        for (i = 0; i < remaining_count; i++) {
                            if (remaining[i] == best) {
                                best_index = i;
                                break;
                            }
                        }
                    } else {
                        best = remaining[0];
                        for (i = 1; i < remaining_count; i++) {
                            if (remaining[i] > best) best = remaining[i];
                        }
                        for (i = 0; i < remaining_count; i++) {
                            if (remaining[i] == best) {
                                best_index = i;
                                break;
                            }
                        }
                    }
                } else {
                    best = remaining[0];
                    best_distance = abs(head - best);
                    for (i = 1; i < remaining_count; i++) {
                        int distance = abs(head - remaining[i]);
                        if (distance < best_distance) {
                            best_distance = distance;
                            best_index = i;
                            best = remaining[i];
                        }
                    }
                }
            }
        }

        movement += abs(head - best);
        head = best;

        for (i = best_index; i + 1 < remaining_count; i++) {
            remaining[i] = remaining[i + 1];
        }
        remaining_count--;
    }

    return movement;
}

static int cache_contains(int *cache, int block)
{
    int i;
    for (i = 0; i < CACHE_SIZE; i++) {
        if (cache[i] == block) return 1;
    }
    return 0;
}

static void cache_insert(int *cache, int block, unsigned int *next_slot)
{
    cache[*next_slot % CACHE_SIZE] = block;
    (*next_slot)++;
}

static void run_configuration(const char *policy_name, int policy,
                              int cache_enabled, int seed, FILE *csv,
                              long *sum_movement, long *sum_latency,
                              long *sum_throughput, long *sum_hit)
{
    request_t requests[REQUEST_COUNT];
    int cache[CACHE_SIZE];
    unsigned int random_state = (unsigned int)seed;
    unsigned int next_slot = 0;
    int hits = 0;
    int i;
    int movement;
    long latency_us;
    long throughput;

    memset(cache, -1, sizeof(cache));
    srand(seed);
    for (i = 0; i < REQUEST_COUNT; i++) {
        requests[i].block = rand() % (DISK_MAX + 1);
    }

    if (cache_enabled) {
        for (i = 0; i < REQUEST_COUNT; i++) {
            if (cache_contains(cache, requests[i].block)) {
                hits++;
            } else {
                cache_insert(cache, requests[i].block, &next_slot);
            }
        }
    }

    movement = policy_movement(policy, 53, requests, REQUEST_COUNT);
    latency_us = (long)movement * 10 + (cache_enabled ? hits * 2 : 0);
    throughput = REQUEST_COUNT * 1000000L / (latency_us > 0 ? latency_us : 1);

    *sum_movement += movement;
    *sum_latency += latency_us / REQUEST_COUNT;
    *sum_throughput += throughput;
    *sum_hit += cache_enabled ? (hits * 100L / REQUEST_COUNT) : 0;

    (void)random_state;

    fprintf(csv, "%s,%s,%d,%d,%ld,%ld,%ld,%ld\n",
            policy_name, cache_enabled ? "on" : "off", seed,
            movement, latency_us / REQUEST_COUNT, throughput,
            (long)(cache_enabled ? hits : 0),
            (long)(cache_enabled ? hits * 100 / REQUEST_COUNT : 0));
}

int main(void)
{
    const char *names[] = {"FCFS", "SCAN", "C-SCAN"};
    FILE *csv = fopen("docs/results/bench_results.csv", "w");
    int policy;
    int cache_enabled;
    int seed;

    if (csv == NULL) {
        fprintf(stderr, "cannot open docs/results/bench_results.csv\n");
        return 1;
    }

    fprintf(csv, "policy,cache,seed,head_movement,avg_latency_us,throughput_requests_per_sec,cache_hits,hit_ratio_percent\n");

    for (policy = 0; policy < 3; policy++) {
        for (cache_enabled = 0; cache_enabled <= 1; cache_enabled++) {
            long sum_movement = 0;
            long sum_latency = 0;
            long sum_throughput = 0;
            long sum_hit = 0;

            for (seed = 0; seed < RUNS; seed++) {
                run_configuration(names[policy], policy, cache_enabled, 700 + seed * 101,
                                  csv, &sum_movement, &sum_latency,
                                  &sum_throughput, &sum_hit);
            }

            printf("%s cache=%s average: movement=%ld latency_us=%ld throughput=%ld hit_ratio=%ld%%\n",
                   names[policy], cache_enabled ? "on" : "off",
                   sum_movement / RUNS, sum_latency / RUNS,
                   sum_throughput / RUNS, sum_hit / RUNS);
        }
    }

    fclose(csv);
    return 0;
}
