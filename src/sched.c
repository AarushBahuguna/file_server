#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>

#include "sched.h"

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

static struct queue_node *queue_pop(void)
{
    struct queue_node *node;

    queue_lock();
    node = g_queue_head;
    if (node != NULL) {
        g_queue_head = node->next;
        if (g_queue_head == NULL) {
            g_queue_tail = NULL;
        }
    }
    queue_unlock();

    return node;
}

static void update_stats_for_request(struct disk_req *req, long enqueue_us)
{
    long long wait_us;
    long movement;

    pthread_mutex_lock(&g_stats_lock);
    wait_us = now_us() - enqueue_us;
    if (wait_us < 0) {
        wait_us = 0;
    }

    movement = labs((long)g_current_head - (long)req->block);
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

        sem_wait(&g_pending_requests);

        while ((node = queue_pop()) == NULL) {
            if (g_shutdown_requested) {
                return NULL;
            }
            sem_wait(&g_pending_requests);
        }

        req = node->req;

        if (req->is_write) {
            disk_raw_write(req->block);
        } else {
            disk_raw_read(req->block);
        }

        update_stats_for_request(req, node->enqueue_us);
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

void sched_set_policy(int policy)
{
    if (policy == SCHED_FCFS || policy == SCHED_SCAN || policy == SCHED_CSCAN) {
        g_policy = policy;
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
}
