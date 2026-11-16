#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>

#include "sched.h"

#define DISK_MIN_BLOCK 0
#define DISK_MAX_BLOCK 199

struct queue_node {
    struct disk_req *req;
    long enqueue_us;
    struct queue_node *next;
};

static struct queue_node *g_queue_head = NULL;
static struct queue_node *g_queue_tail = NULL;
static pthread_mutex_t g_queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_stats_lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t g_pending_requests;
static pthread_t g_scheduler_thread;
static int g_scheduler_started = 0;
static int g_shutdown_requested = 0;
static int g_policy = SCHED_FCFS;
static int g_current_head = 0;
static int g_scan_direction = -1;
static long g_pending_wrap_movement = 0;
static int g_pending_wrap_head = -1;
static struct sched_stats g_stats = {0, 0, 0};

extern int disk_raw_read(int block);
extern int disk_raw_write(int block);

static long long now_us(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return (long long)ts.tv_sec * 1000000LL + (long long)ts.tv_nsec / 1000LL;
}

static void queue_lock(void)
{
    pthread_mutex_lock(&g_queue_lock);
}

static void queue_unlock(void)
{
    pthread_mutex_unlock(&g_queue_lock);
}

static void queue_remove_node(struct queue_node *node, struct queue_node *prev)
{
    if (prev == NULL) {
        g_queue_head = node->next;
    } else {
        prev->next = node->next;
    }

    if (g_queue_tail == node) {
        g_queue_tail = prev;
    }

    node->next = NULL;
}

int sched_pick_next(const int *queue, int count, int head, int direction,
                   int policy, int *selected_block)
{
    int i;
    int best_index = -1;
    int best_distance = INT_MAX;
    int use_direction = direction;

    if (queue == NULL || count <= 0 || selected_block == NULL) {
        return -EINVAL;
    }

    if (policy == SCHED_FCFS) {
        *selected_block = queue[0];
        return 0;
    }

    for (i = 0; i < count; i++) {
        if ((use_direction > 0 && queue[i] >= head) ||
            (use_direction < 0 && queue[i] <= head)) {
            int distance = abs(head - queue[i]);
            if (distance < best_distance) {
                best_distance = distance;
                best_index = i;
            }
        }
    }

    if (best_index == -1 && policy == SCHED_SCAN) {
        use_direction *= -1;
        for (i = 0; i < count; i++) {
            if ((use_direction > 0 && queue[i] >= head) ||
                (use_direction < 0 && queue[i] <= head)) {
                int distance = abs(head - queue[i]);
                if (distance < best_distance) {
                    best_distance = distance;
                    best_index = i;
                }
            }
        }
    }

    if (best_index == -1 && policy == SCHED_CSCAN) {
        if (use_direction > 0) {
            int smallest = queue[0];
            for (i = 1; i < count; i++) {
                if (queue[i] < smallest) {
                    smallest = queue[i];
                }
            }
            *selected_block = smallest;
            return 0;
        }

        int largest = queue[0];
        for (i = 1; i < count; i++) {
            if (queue[i] > largest) {
                largest = queue[i];
            }
        }
        *selected_block = largest;
        return 0;
    }

    if (best_index == -1) {
        return -EINVAL;
    }

    *selected_block = queue[best_index];
    return 0;
}

static int queue_push(struct disk_req *req, long enqueue_us)
{
    struct queue_node *node;

    if (req == NULL) {
        return -EINVAL;
    }

    node = calloc(1, sizeof(*node));
    if (node == NULL) {
        return -ENOMEM;
    }

    node->req = req;
    node->enqueue_us = enqueue_us;
    node->next = NULL;

    queue_lock();
    if (g_queue_tail != NULL) {
        g_queue_tail->next = node;
    } else {
        g_queue_head = node;
    }
    g_queue_tail = node;
    queue_unlock();

    sem_post(&g_pending_requests);
    return 0;
}

static struct queue_node *pick_next_node(void)
{
    struct queue_node *cur;
    struct queue_node *prev;
    struct queue_node *best = NULL;
    struct queue_node *best_prev = NULL;
    int best_distance = INT_MAX;
    int direction = g_scan_direction;

    g_pending_wrap_movement = 0;
    g_pending_wrap_head = -1;

    if (g_queue_head == NULL) {
        return NULL;
    }

    if (g_policy == SCHED_FCFS) {
        best = g_queue_head;
        queue_remove_node(best, NULL);
        return best;
    }

    for (cur = g_queue_head, prev = NULL; cur != NULL; prev = cur, cur = cur->next) {
        int request_block = cur->req->block;
        int in_direction = 0;

        if (g_policy == SCHED_SCAN) {
            in_direction = (direction > 0) ? (request_block >= g_current_head) : (request_block <= g_current_head);
        } else {
            in_direction = (direction > 0) ? (request_block >= g_current_head) : (request_block <= g_current_head);
        }

        if (in_direction) {
            int distance = abs(g_current_head - request_block);
            if (distance < best_distance) {
                best = cur;
                best_prev = prev;
                best_distance = distance;
            }
        }
    }

    if (best == NULL && g_policy == SCHED_SCAN) {
        if (direction > 0) {
            g_pending_wrap_movement = DISK_MAX_BLOCK - g_current_head;
            g_pending_wrap_head = DISK_MAX_BLOCK;
            g_scan_direction = -1;
        } else {
            g_pending_wrap_movement = g_current_head - DISK_MIN_BLOCK;
            g_pending_wrap_head = DISK_MIN_BLOCK;
            g_scan_direction = 1;
        }

        g_current_head = g_pending_wrap_head;
        direction = g_scan_direction;
        for (cur = g_queue_head, prev = NULL; cur != NULL; prev = cur, cur = cur->next) {
            int request_block = cur->req->block;
            int in_direction_after_reverse = (direction > 0) ? (request_block >= g_current_head) : (request_block <= g_current_head);
            if (in_direction_after_reverse) {
                int distance = abs(g_current_head - request_block);
                if (distance < best_distance) {
                    best = cur;
                    best_prev = prev;
                    best_distance = distance;
                }
            }
        }
    }

    if (best == NULL && g_policy == SCHED_CSCAN) {
        if (direction > 0) {
            struct queue_node *smallest = NULL;
            struct queue_node *smallest_prev = NULL;

            g_pending_wrap_movement = (DISK_MAX_BLOCK - g_current_head) + DISK_MAX_BLOCK;
            g_pending_wrap_head = DISK_MIN_BLOCK;

            for (cur = g_queue_head, prev = NULL; cur != NULL; prev = cur, cur = cur->next) {
                if (smallest == NULL || cur->req->block < smallest->req->block) {
                    smallest = cur;
                    smallest_prev = prev;
                }
            }

            best = smallest;
            best_prev = smallest_prev;
        } else {
            struct queue_node *largest = NULL;
            struct queue_node *largest_prev = NULL;

            g_pending_wrap_movement = g_current_head + DISK_MAX_BLOCK;
            g_pending_wrap_head = DISK_MAX_BLOCK;

            for (cur = g_queue_head, prev = NULL; cur != NULL; prev = cur, cur = cur->next) {
                if (largest == NULL || cur->req->block > largest->req->block) {
                    largest = cur;
                    largest_prev = prev;
                }
            }

            best = largest;
            best_prev = largest_prev;
        }
    }

    if (best == NULL) {
        best = g_queue_head;
        best_prev = NULL;
    }

    queue_remove_node(best, best_prev);
    return best;
}

static void update_stats_for_request(struct disk_req *req, long enqueue_us, long extra_movement)
{
    long long wait_us;
    long movement;

    pthread_mutex_lock(&g_stats_lock);
    wait_us = now_us() - enqueue_us;
    if (wait_us < 0) {
        wait_us = 0;
    }

    movement = extra_movement + labs((long)g_current_head - (long)req->block);
    g_stats.total_head_movement += movement;
    g_current_head = req->block;
    g_stats.requests_served += 1;
    g_stats.total_wait_us += (long)wait_us;
    pthread_mutex_unlock(&g_stats_lock);
}

static void *scheduler_loop(void *arg)
{
    (void)arg;

    while (1) {
        struct queue_node *node;
        struct disk_req *req;
        long extra_movement = 0;

        sem_wait(&g_pending_requests);

        while ((node = pick_next_node()) == NULL) {
            if (g_shutdown_requested) {
                return NULL;
            }
            sem_wait(&g_pending_requests);
        }

        req = node->req;
        extra_movement = g_pending_wrap_movement;
        if (extra_movement > 0) {
            g_current_head = g_pending_wrap_head;
        }
        g_pending_wrap_movement = 0;
        g_pending_wrap_head = -1;

        if (req->is_write) {
            disk_raw_write(req->block);
        } else {
            disk_raw_read(req->block);
        }

        update_stats_for_request(req, node->enqueue_us, extra_movement);
        req->status = 0;
        sem_post(&req->done);
        free(node);

        if (g_shutdown_requested && g_queue_head == NULL) {
            return NULL;
        }
    }

    return NULL;
}

int sched_init(void)
{
    if (g_scheduler_started) {
        return 0;
    }

    g_shutdown_requested = 0;
    g_queue_head = NULL;
    g_queue_tail = NULL;
    g_current_head = 0;
    g_scan_direction = -1;
    g_pending_wrap_movement = 0;
    g_pending_wrap_head = -1;
    g_stats.total_head_movement = 0;
    g_stats.requests_served = 0;
    g_stats.total_wait_us = 0;

    if (sem_init(&g_pending_requests, 0, 0) != 0) {
        return -1;
    }

    if (pthread_create(&g_scheduler_thread, NULL, scheduler_loop, NULL) != 0) {
        sem_destroy(&g_pending_requests);
        return -1;
    }

    g_scheduler_started = 1;
    return 0;
}

int sched_submit(struct disk_req *r)
{
    int rc;

    if (r == NULL || !g_scheduler_started) {
        return -1;
    }

    r->status = 0;
    rc = queue_push(r, now_us());
    if (rc != 0) {
        r->status = rc;
        return rc;
    }

    sem_wait(&r->done);
    return r->status;
}

void sched_set_head(int head)
{
    if (head < DISK_MIN_BLOCK) {
        g_current_head = DISK_MIN_BLOCK;
    } else if (head > DISK_MAX_BLOCK) {
        g_current_head = DISK_MAX_BLOCK;
    } else {
        g_current_head = head;
    }
}

void sched_set_policy(int policy)
{
    if (policy == SCHED_FCFS || policy == SCHED_SCAN || policy == SCHED_CSCAN) {
        g_policy = policy;
        if (policy == SCHED_SCAN) {
            g_scan_direction = -1;
        } else if (policy == SCHED_CSCAN) {
            g_scan_direction = 1;
        }
    }
}

void sched_stats(struct sched_stats *out)
{
    if (out == NULL) {
        return;
    }

    pthread_mutex_lock(&g_stats_lock);
    *out = g_stats;
    pthread_mutex_unlock(&g_stats_lock);
}

void sched_shutdown(void)
{
    if (!g_scheduler_started) {
        return;
    }

    g_shutdown_requested = 1;
    sem_post(&g_pending_requests);

    pthread_join(g_scheduler_thread, NULL);
    sem_destroy(&g_pending_requests);
    g_scheduler_started = 0;
    g_queue_head = NULL;
    g_queue_tail = NULL;
    g_pending_wrap_movement = 0;
    g_pending_wrap_head = -1;
}
