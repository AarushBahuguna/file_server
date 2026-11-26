#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include "threadpool.h"
#include "dispatcher.h"

static request_t queue[QUEUE_SIZE];
static int front, rear, count;
static pthread_t workers[MAX_WORKERS];
static int worker_count;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t empty_slots, full_slots;
static int shutdown_flag;

static void *worker(void *arg) {
    (void)arg;
    for (;;) {
        sem_wait(&full_slots);
        pthread_mutex_lock(&queue_mutex);
        if (shutdown_flag && count == 0) { pthread_mutex_unlock(&queue_mutex); break; }
        request_t req = {0};
        if (count > 0) { req = queue[front]; front = (front + 1) % QUEUE_SIZE; count--; }
        pthread_mutex_unlock(&queue_mutex);
        sem_post(&empty_slots);
        if (req.cmd != CMD_NONE) dispatch_request(&req);
    }
    return NULL;
}

int threadpool_init(int workers_count) {
    if (workers_count <= 0 || workers_count > MAX_WORKERS) return -EINVAL;
    front = rear = count = shutdown_flag = 0;
    worker_count = workers_count;
    if (sem_init(&empty_slots, 0, QUEUE_SIZE) != 0) return -1;
    if (sem_init(&full_slots, 0, 0) != 0) { sem_destroy(&empty_slots); return -1; }
    for (int i = 0; i < worker_count; i++) if (pthread_create(&workers[i], NULL, worker, NULL) != 0) return -1;
    return 0;
}

int threadpool_submit(request_t *request) {
    if (!request) return -EINVAL;
    sem_wait(&empty_slots);
    pthread_mutex_lock(&queue_mutex);
    if (shutdown_flag) { pthread_mutex_unlock(&queue_mutex); sem_post(&empty_slots); return -EPIPE; }
    queue[rear] = *request; rear = (rear + 1) % QUEUE_SIZE; count++;
    pthread_mutex_unlock(&queue_mutex); sem_post(&full_slots); return 0;
}

void threadpool_shutdown(void) {
    pthread_mutex_lock(&queue_mutex); shutdown_flag = 1; pthread_mutex_unlock(&queue_mutex);
    for (int i = 0; i < worker_count; i++) sem_post(&full_slots);
    for (int i = 0; i < worker_count; i++) pthread_join(workers[i], NULL);
    sem_destroy(&empty_slots); sem_destroy(&full_slots);
}
