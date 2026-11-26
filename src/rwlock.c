#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include "rwlock.h"
#include "waitfor.h"

#define LOCK_TABLE_SIZE 257

typedef struct FileLock {
    int file_id, readers, writer, waiting_readers, waiting_writers;
    int reader_sessions[MAX_SESSIONS];
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct FileLock *next;
} FileLock;

static FileLock *table[LOCK_TABLE_SIZE];
static pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;
static int policy = FAIR_POLICY;

static unsigned hash_id(int file_id) { return (unsigned)file_id % LOCK_TABLE_SIZE; }

static FileLock *get_lock(int file_id) {
    unsigned h = hash_id(file_id);
    pthread_mutex_lock(&table_mutex);
    for (FileLock *lock = table[h]; lock; lock = lock->next) if (lock->file_id == file_id) { pthread_mutex_unlock(&table_mutex); return lock; }
    FileLock *lock = calloc(1, sizeof(*lock));
    if (!lock) { pthread_mutex_unlock(&table_mutex); return NULL; }
    lock->file_id = file_id; pthread_mutex_init(&lock->mutex, NULL); pthread_cond_init(&lock->cond, NULL); lock->next = table[h]; table[h] = lock;
    pthread_mutex_unlock(&table_mutex); return lock;
}

int lock_manager_init(void) { pthread_mutex_lock(&table_mutex); policy = FAIR_POLICY; pthread_mutex_unlock(&table_mutex); wfg_init(); return 0; }

int lock_acquire(int file_id, int mode, int session) {
    if (mode != LOCK_READ && mode != LOCK_WRITE) return -EINVAL;
    if (session < 0) return -EINVAL;
    FileLock *lock = get_lock(file_id); if (!lock) return -ENOMEM;
    pthread_mutex_lock(&lock->mutex);
    if (mode == LOCK_READ) lock->waiting_readers++; else lock->waiting_writers++;

    int holders[MAX_SESSIONS];
    int holder_count = 0;
    if (lock->writer != 0) holders[holder_count++] = lock->writer;
    for (int i = 0; i < lock->readers && holder_count < MAX_SESSIONS; i++) {
        holders[holder_count++] = lock->reader_sessions[i];
    }
    wfg_on_wait(session, file_id, holders, holder_count);

    for (;;) {
        if (wfg_is_aborted(session)) {
            if (mode == LOCK_READ) lock->waiting_readers--; else lock->waiting_writers--;
            wfg_on_release(session);
            pthread_cond_broadcast(&lock->cond); pthread_mutex_unlock(&lock->mutex); return -EDEADLK;
        }
        int can_acquire;
        if (mode == LOCK_READ) {
            if (policy == READER_PREFERENCE) can_acquire = lock->writer == 0;
            else can_acquire = lock->writer == 0 && lock->waiting_writers == 0;
        } else can_acquire = lock->writer == 0 && lock->readers == 0;
        if (can_acquire) break;

        struct timespec timeout; clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_nsec += 100000000L; if (timeout.tv_nsec >= 1000000000L) { timeout.tv_sec++; timeout.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&lock->cond, &lock->mutex, &timeout);
    }

    if (mode == LOCK_READ) {
        lock->waiting_readers--;
        lock->readers++;
        lock->reader_sessions[lock->readers - 1] = session;
    }
    else { lock->waiting_writers--; lock->writer = session; }
    wfg_on_grant(session);
    pthread_mutex_unlock(&lock->mutex); return 0;
}

void lock_release(int file_id, int session) {
    FileLock *lock = get_lock(file_id); if (!lock) return;
    pthread_mutex_lock(&lock->mutex);
    if (lock->writer == session) {
        lock->writer = 0;
    } else {
        for (int i = 0; i < lock->readers; i++) {
            if (lock->reader_sessions[i] == session) {
                for (int j = i; j + 1 < lock->readers; j++) {
                    lock->reader_sessions[j] = lock->reader_sessions[j + 1];
                }
                lock->readers--;
                break;
            }
        }
    }
    wfg_on_release(session);
    pthread_cond_broadcast(&lock->cond); pthread_mutex_unlock(&lock->mutex);
}

void lock_set_policy(int policy_value) {
    if (policy_value < READER_PREFERENCE || policy_value > FAIR_POLICY) return;
    pthread_mutex_lock(&table_mutex); policy = policy_value; pthread_mutex_unlock(&table_mutex);
}

void lock_manager_destroy(void) {
    pthread_mutex_lock(&table_mutex);
    for (int i = 0; i < LOCK_TABLE_SIZE; i++) { FileLock *cur = table[i]; while (cur) { FileLock *next = cur->next; pthread_cond_destroy(&cur->cond); pthread_mutex_destroy(&cur->mutex); free(cur); cur = next; } table[i] = NULL; }
    pthread_mutex_unlock(&table_mutex); wfg_destroy();
}
