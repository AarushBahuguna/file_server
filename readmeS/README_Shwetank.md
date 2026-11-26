# Shwetank Synchronization & Server Layer — Integrated README

## Overview

Shwetank's server and synchronization code is integrated with Aarush's scheduler and
wait-for graph plus Arjun's storage API. The implementation provides bounded worker
queueing, FIFO server startup, command dispatch, per-file reader-writer locking,
LRU caching, dirty write-back, and cache statistics.

## Integrated files

| File | Purpose |
|------|---------|
| `include/server.h` | Server FIFO and session constants. |
| `include/threadpool.h` | Bounded worker-pool API. |
| `include/dispatcher.h` | Command dispatch API. |
| `include/rwlock.h` | Reader-writer lock API. |
| `include/cache.h` | LRU cache API and statistics. |
| `src/server.c` | FIFO listener and shutdown orchestration. |
| `src/threadpool.c` | Semaphore-bounded producer/consumer worker pool. |
| `src/dispatcher.c` | Command parsing and filesystem/cache calls. |
| `src/rwlock.c` | Per-file fair/reader/writer locking with abort checks. |
| `src/cache.c` | 64-entry LRU cache, write-back, and stats. |
| `tests/test_rwlock.c` | Reader/writer lock test. |
| `tests/test_cache.c` | Cache hit/miss/writeback test. |

## API adaptations

### Scheduler compatibility

Cache disk requests use Aarush's existing request type:

```c
struct disk_req {
    int block;
    int is_write;
    void *buf;
    sem_t done;
    int status;
};
```

Cache misses call `sched_submit()`. Dirty evictions and flushes call
`sched_submit()` as writes.

### Wait-for compatibility

Lock waits call:

```c
wfg_on_wait(session, file_id, holders, holder_count);
```

Lock grants call `wfg_on_grant(session)`, and lock releases call
`wfg_on_release(session)`. Victims return `-EDEADLK` through the existing
`wfg_is_aborted()` recovery mechanism.

## Request flow

```text
Future FIFO client request
  -> server FIFO
  -> bounded worker queue
  -> dispatcher
  -> reader/writer lock
  -> Arjun fs_* operation
  -> Shwetank cache
  -> Aarush sched_submit
  -> Arjun disk_raw_* operation
```

## Locking

The lock manager supports reader preference, writer preference, and fair policy.
Waiting uses a timed condition-variable wait and can detect an aborted session.
Reader ownership is tracked by session ID, unlike the original starter count-only
implementation.

The integrated code follows the requested ordering direction:

```text
file lock -> filesystem metadata -> cache -> scheduler queue
```

## Cache behavior

- 64 entries
- doubly linked LRU list
- hash lookup
- dirty write-back on eviction and flush
- hits, misses, evictions, and writebacks counted
- cache mutex is not held during disk I/O

This allows several cache misses to reach the scheduler queue so SCAN and C-SCAN
have meaningful workloads.

## Shutdown order

```text
stop accepting requests
  -> drain/join workers
  -> cache_flush
  -> disk_save
  -> cache_destroy
  -> sched_shutdown
  -> lock_manager_destroy
  -> fs_unmount
```

## Build and test

```bash
cd /home/aarushb/Documents/file_server/file_server

gcc -std=c11 -Wall -Wextra -Iinclude -Isrc \
    src/disk.c src/sched.c src/waitfor.c src/cache.c tests/test_cache.c \
    -lpthread -o test_cache
./test_cache

gcc -std=c11 -Wall -Wextra -Iinclude -Isrc \
    src/waitfor.c src/rwlock.c tests/test_rwlock.c \
    -lpthread -o test_rwlock
./test_rwlock

gcc -std=c11 -Wall -Wextra -Iinclude -Isrc \
    src/disk.c src/alloc.c src/dir.c src/fs.c src/sched.c src/waitfor.c \
    src/threadpool.c src/dispatcher.c src/rwlock.c src/cache.c src/server.c \
    -lpthread -o server
```

## Shubham future integration boundary

Shubham's future code should provide:

- protocol request parsing and line framing
- `/tmp/fs_req` server request construction
- `/tmp/fs_resp_<session_id>` reply handling
- PID/counter session IDs
- `HELLO`/`BYE` session lifecycle
- FIFO cleanup and SIGINT/SIGTERM handling
- thread-safe logger calls

The server and dispatcher currently reserve that boundary and are protected against
FIFO EOF and SIGPIPE. Shubham's code should not require changes to the scheduler,
wait-for graph, or filesystem internals.

## Known starter-code limits

- The dispatcher still needs complete file-descriptor to inode lock routing.
- Session/table access is synchronized, but no Shubham protocol module exists yet.
- The lock table needs reference-count cleanup for production use.
- Cache loading can need an explicit per-entry loading state for true concurrent miss
  de-duplication.
- ThreadSanitizer is the next validation step for server stress and cache access.
