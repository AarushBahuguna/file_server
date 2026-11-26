#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "rwlock.h"

static pthread_t readers[8];
static int max_readers;
static pthread_mutex_t state = PTHREAD_MUTEX_INITIALIZER;

static void *reader_thread(void *arg) {
    int session = *(int *)arg;
    if (lock_acquire(10, LOCK_READ, session) != 0) return NULL;
    pthread_mutex_lock(&state); max_readers++; pthread_mutex_unlock(&state);
    {
        struct timespec req = {0};
        req.tv_sec = 0;
        req.tv_nsec = 3000;
        nanosleep(&req, NULL);
    }
    pthread_mutex_lock(&state); max_readers--; pthread_mutex_unlock(&state);
    lock_release(10, session);
    return NULL;
}

int main(void) {
    int session_ids[8];
    if (lock_manager_init() != 0) return 1;
    for (int i = 0; i < 8; i++) { session_ids[i] = i + 1; pthread_create(&readers[i], NULL, reader_thread, &session_ids[i]); }
    for (int i = 0; i < 8; i++) pthread_join(readers[i], NULL);
    if (max_readers != 0) { fprintf(stderr, "reader lock leak detected\n"); lock_manager_destroy(); return 1; }
    if (lock_acquire(11, LOCK_WRITE, 20) != 0) { fprintf(stderr, "writer acquisition failed\n"); lock_manager_destroy(); return 1; }
    lock_release(11, 20);
    lock_manager_destroy();
    puts("rwlock reader and writer checks passed");
    return 0;
}
