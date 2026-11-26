# Aarush's Deadlock & Disk-Scheduling Module

## What This Is

This is the **deadlock detection** and **disk scheduling** component of a
multi-user file system project (OS course). It provides two modules:

1. **Disk Scheduler** (`include/sched.h` / `src/sched.c`)
   — queues disk I/O requests and services them using one of three
   policies: FCFS, SCAN, or C-SCAN.

2. **Wait-For Graph** (`include/waitfor.h` / `src/waitfor.c`)
   — tracks which sessions are waiting for which locks, detects
   deadlocks (cycles), and picks a victim to abort.

---

## Day 2 Deliverables (Implementation)

Day 2 is about the **request queue and FCFS scheduler**. The headers from
Day 1 are already in place, and now the system starts to become real.

| File | Purpose |
|------|---------|
| `tests/mock_disk.c` | Provides a fake disk layer with `disk_raw_read` and `disk_raw_write` that records block numbers instead of doing real I/O. |
| `src/sched.c` | Implements the request queue, scheduler thread loop, and FCFS scheduling logic. |
| `tests/test_sched.c` | Contains the first test suite: 20 requests served in arrival order, exact-once service, and hand-checked movement totals. |

---

## File Structure

```
os-project/
├── include/
│   ├── sched.h            ← scheduler API (Day 1) ✅
│   └── waitfor.h          ← wait-for graph API (Day 1) ✅
├── src/
│   ├── sched.c            ← scheduler implementation (Day 2) ✅
│   ├── waitfor.c          ← deadlock detector (Day 4-5)
│   └── bench.c            ← benchmarking module (Day 8)
├── tests/
│   ├── mock_disk.c        ← fake disk for unit testing (Day 2) ✅
│   ├── test_sched.c       ← scheduler tests (Day 2-3)
│   └── test_deadlock.c    ← deadlock tests (Day 5-6)
├── scripts/               ← demo & bench scripts (Day 8)
├── docs/                  ← report and results (Day 9)
└── README.md              ← project overview (Day 1)
```

---

## Understanding the Code

### mock_disk.c — Fake Disk Interface

| Item | What It Is |
|------|------------|
| `disk_raw_read(block)` | Simulates a disk read and records the block number the scheduler asked for. |
| `disk_raw_write(block)` | Simulates a disk write and records the block number. |
| `disk_state` | Minimal fake-disk state used to verify the scheduler is calling the correct block. |

The mock disk is intentionally simple: it does not model real timing or data
transfer; it only verifies that requests are being issued in the right order and
that no request is lost or duplicated.

### sched.c — Request Queue and Scheduler Thread

| Item | What It Is |
|------|------------|
| Request queue | A linked list or queue of pending `struct disk_req` entries protected by a mutex. |
| Pending-request semaphore | A counting semaphore used to wake the scheduler thread whenever a new request arrives. |
| Scheduler thread loop | Waits for work, removes the next request, services it, updates head position, and posts `done`. |
| `head` | Current disk head position. |
| `head_movement` | Total seek distance accumulated across all serviced requests. |
| `requests_served` | Number of requests processed. |
| `total_wait_time` | Sum of queue wait time for requests. |

### test_sched.c — First Real Scheduler Tests

The first validation is deliberately simple:

- 20 requests are submitted into the queue
- the scheduler services them in arrival order
- each request is completed exactly once
- total head movement matches a manual calculation

This ensures the queue, scheduler loop, and completion signaling work before we
move on to SCAN and C-SCAN.

---

## How to Compile (Day 2)

The implementation is now in place, so you can compile the scheduler and the
mock-disk test directly.

```bash
# Compile the Day 2 scheduler test
gcc -Wall -Wextra -I include \
    src/sched.c tests/mock_disk.c tests/test_sched.c \
    -lpthread -o test_sched

./test_sched
```

### Verify the Day 2 implementation

```bash
gcc -Wall -Wextra -I include \
    src/sched.c tests/mock_disk.c tests/test_sched.c \
    -lpthread -o test_sched
```

This should build cleanly with no warnings and no errors.

---

## Key Design Decisions

- **Queue behind a mutex** — The pending request list must be protected from
  concurrent producers and the worker thread.
- **Semaphore for wakeups** — The scheduler blocks on a counting semaphore until a
  request is available.
- **FCFS as the first policy** — Day 2 only needs a correct queue and a simple
  "head of list" pick order.
- **No scheduler access after completion** — The request object lives on the caller's
  stack, so the scheduler must copy everything it needs before posting `done`.
- **Head movement counters** — These are part of the scheduler contract and are used
  to compare against textbook values later.

---

## Team Integration Points

| Hook call site (in cache.c) | When to call |
|------------------------------|--------------|
| `sched_submit(&req)` | On a cache miss or dirty eviction — the request goes to the disk scheduler |

| Dependency | Why it matters |
|------------|----------------|
| `disk_raw_read` / `disk_raw_write` | The scheduler thread calls these on the fake disk during testing |
| `rwlock.c` | Later will call the deadlock graph hooks once the wait-for logic is implemented |

---

## What's Next (Day 3)

- `pick_next(queue, head, direction, policy)` as a pure function
- SCAN and C-SCAN logic
- Small seek delay or batch window so multiple requests can overlap in the queue
- Add the textbook example:
  - head = 53
  - queue = 98 183 37 122 14 124 65 67
  - FCFS -> 640
  - SCAN -> 236
  - C-SCAN -> 382

---

## Definition of Done for Day 2

- [x] Mock disk records read/write block requests correctly
- [x] Scheduler thread processes queued work in arrival order
- [x] Exactly 20 requests are served without loss or duplication
- [x] Total head movement matches a manual calculation
- [x] `done` is signaled after the scheduler services the request
- [x] The queue and scheduler are ready for SCAN / C-SCAN on Day 3

### Verification Result

The Day 2 implementation was compiled and executed successfully:

```bash
cd /home/aarushb/Documents/file_server/file_server && gcc -Wall -Wextra -I include src/sched.c tests/mock_disk.c tests/test_sched.c -lpthread -o test_sched && ./test_sched
```

Output:

```text
scheduler fcfs test passed
```
