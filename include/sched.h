/*
 * sched.h - Disk Scheduler API
 * -----------------------------
 * This header defines the interface for the disk-scheduling module.
 *
 * How it works (big picture):
 *   - A client thread that needs to read/write a disk block fills in a
 *     disk_req struct and calls sched_submit().  The call blocks until the
 *     scheduler thread has serviced the request.
 *   - A background scheduler thread picks the next request from a queue
 *     according to the active policy (FCFS, SCAN, or C-SCAN), calls the
 *     low-level disk I/O, and then wakes the waiting client.
 *   - sched_set_policy() lets you switch the algorithm at runtime.
 *   - sched_stats() lets you read performance counters (total head
 *     movement, requests served, etc.) at any time.
 *
 * Files that implement this header:
 *   src/sched.c        - the actual scheduler logic and thread
 *   tests/mock_disk.c  - a fake disk back-end used in unit tests
 */

#ifndef SCHED_H   /* include guard - prevents double inclusion */
#define SCHED_H

#include <semaphore.h>  /* for sem_t (POSIX semaphore) */

/* ---------------------------------------------------------------
 * Scheduling policies — passed to sched_set_policy()
 *
 *   SCHED_FCFS  : First-Come, First-Served.  Requests are served in
 *                 the order they arrive.  Simple but can cause large
 *                 head movements.
 *
 *   SCHED_SCAN  : Elevator algorithm.  The disk head moves in one
 *                 direction, serving requests along the way, then
 *                 reverses at the end of the disk.
 *
 *   SCHED_CSCAN : Circular SCAN.  Like SCAN but only serves requests
 *                 in one direction; when the head reaches the end it
 *                 jumps back to the beginning without serving.
 * --------------------------------------------------------------- */
#define SCHED_FCFS  0   /* first-come first-served */
#define SCHED_SCAN  1   /* elevator (SCAN) */
#define SCHED_CSCAN 2   /* circular-SCAN (C-SCAN) */

/* ---------------------------------------------------------------
 * struct disk_req — one pending disk I/O request
 *
 * A client thread allocates this on its own stack, fills in the
 * first three fields, initialises the semaphore, and passes a
 * pointer to sched_submit().
 * --------------------------------------------------------------- */
struct disk_req {
    int    block;      /* which disk block to read or write (0-based) */
    int    is_write;   /* 0 = read, 1 = write */
    void  *buf;        /* pointer to a data buffer for the I/O */
    sem_t  done;       /* semaphore: scheduler posts this when the
                          request has been serviced, so the waiting
                          client thread wakes up */
    int    status;     /* result code set by the scheduler:
                          0  = success
                          <0 = error (e.g., -EIO) */
};

/* ---------------------------------------------------------------
 * struct sched_stats — performance counters
 *
 * Filled in by sched_stats().  All counters are cumulative since the
 * scheduler was started.
 * --------------------------------------------------------------- */
struct sched_stats {
    long total_head_movement;  /* sum of |head - block| for every
                                  request served */
    int  requests_served;      /* how many requests completed so far */
    long total_wait_us;        /* total time (microseconds) all
                                  requests spent waiting in the queue */
};

/* ---------------------------------------------------------------
 * sched_init  — start the scheduler
 *
 * Call once at program start.  Creates the scheduler thread and
 * initialises the request queue.  The default policy is FCFS.
 *
 * Returns 0 on success, -1 on error.
 * --------------------------------------------------------------- */
int sched_init(void);

/* ---------------------------------------------------------------
 * sched_submit — submit a disk request and wait for it
 *
 * The caller fills in r->block, r->is_write, and r->buf, then calls
 * this function.  The function:
 *   1. Adds the request to the scheduler's internal queue.
 *   2. Signals the scheduler thread that work is available.
 *   3. Blocks (waits on r->done) until the scheduler services it.
 *
 * Returns r->status (0 on success, negative on error).
 * --------------------------------------------------------------- */
int sched_submit(struct disk_req *r);

/* ---------------------------------------------------------------
 * sched_set_policy — change the scheduling algorithm
 *
 * Can be called at any time.  The new policy takes effect for the
 * next request the scheduler picks from the queue.
 *
 * policy: one of SCHED_FCFS, SCHED_SCAN, or SCHED_CSCAN.
 * --------------------------------------------------------------- */
void sched_set_policy(int policy);

/* ---------------------------------------------------------------
 * sched_stats — read current performance counters
 *
 * Copies the scheduler's internal counters into *out.  This is safe
 * to call from any thread at any time.
 * --------------------------------------------------------------- */
void sched_stats(struct sched_stats *out);

/* ---------------------------------------------------------------
 * sched_shutdown — stop the scheduler
 *
 * Signals the scheduler thread to exit and waits for it to finish.
 * Any requests still in the queue will be serviced before the thread
 * terminates.
 * --------------------------------------------------------------- */
void sched_shutdown(void);

#endif /* SCHED_H */
