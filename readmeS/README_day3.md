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

## Day 3 Deliverables (SCAN and C-SCAN)

Day 3 is about the **policy logic itself**. The queue and FCFS queue order are
already working, so now we move to the textbook scheduling cases for SCAN and
C-SCAN.

| File | Purpose |
|------|---------|
| `include/sched.h` | Adds the head-setting API and a pure request-selection helper used for deterministic policy tests. |
| `src/sched.c` | Implements SCAN / C-SCAN selection, direction tracking, and end-of-disk stop handling. |
| `tests/test_sched.c` | Validates FCFS, SCAN, and C-SCAN against the known textbook values. |

---

## File Structure

```
os-project/
├── include/
│   ├── sched.h            ← scheduler API (Day 1) ✅
│   └── waitfor.h          ← wait-for graph API (Day 1) ✅
├── src/
│   ├── sched.c            ← scheduler implementation (Day 2-3) ✅
│   ├── waitfor.c          ← deadlock detector (Day 4-5)
│   └── bench.c            ← benchmarking module (Day 8)
├── tests/
│   ├── mock_disk.c        ← fake disk for unit testing (Day 2) ✅
│   ├── test_sched.c       ← scheduler tests (Day 2-3) ✅
│   └── test_deadlock.c    ← deadlock tests (Day 5-6)
├── scripts/               ← demo & bench scripts (Day 8)
├── docs/                  ← report and results (Day 9)
└── README.md              ← project overview (Day 1)
```

---

## Understanding the Code

### sched.h — Scheduler API and policy helper

| Item | What It Is |
|------|------------|
| `sched_set_policy(policy)` | Switches between FCFS, SCAN, and C-SCAN. |
| `sched_set_head(head)` | Sets the initial disk head position for deterministic testing. |
| `sched_pick_next(queue, count, head, direction, policy, &selected)` | Pure helper that chooses the next request without touching the queue state. |
| `sched_submit(r)` | Adds a request and blocks until it is served. |
| `sched_stats(out)` | Returns current counters such as head movement and requests served. |

### sched.c — SCAN and C-SCAN logic

| Item | What It Is |
|------|------------|
| `g_scan_direction` | Tracks the current direction of the disk head for SCAN / C-SCAN. |
| `pick_next_node()` | Chooses the next request from the pending queue based on policy and direction. |
| `g_pending_wrap_movement` | Records extra boundary cost for C-SCAN wrap-around or SCAN end-stop reversal. |
| `update_stats_for_request()` | Adds the correct movement plus queue wait time to the scheduler counters. |

### test_sched.c — textbook verification

The Day 3 test validates the known textbook example:

- head = 53
- queue = 98, 183, 37, 122, 14, 124, 65, 67
- disk = 0..199

Expected totals:

- FCFS -> 640
- SCAN (moving down first) -> 236
- C-SCAN (moving up) -> 382

---

## How to Compile and Run

```bash
# Compile the Day 3 scheduler tests
cd /home/aarushb/Documents/file_server/file_server
gcc -Wall -Wextra -I include \
    src/sched.c tests/mock_disk.c tests/test_sched.c \
    -lpthread -o test_sched

./test_sched
```

---

## Key Design Decisions

- **Pure policy helper** — The next-request decision logic is separated from the
  keyboard-style queue management so the policy can be tested directly.
- **Real disk-end stops** — SCAN and C-SCAN both treat the disk edges as real
  stop points; this is essential for matching the textbook totals.
- **C-SCAN wrap cost** — The return jump to the start of the disk is counted as
  movement, matching the expected 382 total.
- **Deterministic testing** — A settable initial head and a pure `pick_next`
  helper make the textbook case reproducible.
- **Head movement counters** — Movement is counted as actual disk travel, including
  end-of-disk reversals and circular wrap-around.

---

## Team Integration Points

| Scheduler call site (in cache.c) | When to call |
|-----------------------------------|--------------|
| `sched_submit(&req)` | On a cache miss or dirty eviction — the request goes to the disk scheduler |

| Dependency | Why it matters |
|------------|----------------|
| `disk_raw_read` / `disk_raw_write` | The scheduler thread calls these to simulate actual disk access |
| `rwlock.c` | Later will connect into the deadlock graph once Day 4 starts |

---

## What's Next (Day 4)

- build the wait-for graph data structure
- track waiter → holder edges
- detect cycles from a blocked lock request
- add the deadlock detector and recovery logic

---

## Definition of Done for Day 3

- [x] FCFS still works for the Day 2 queue and scheduler flow
- [x] SCAN chooses the nearest request in the current direction and reverses at the end
- [x] C-SCAN sweeps in one direction, then wraps back to the start and counts the return jump
- [x] The disk ends are treated as real stops in the movement calculation
- [x] The textbook example validates the expected totals:
  - FCFS = 640
  - SCAN = 236
  - C-SCAN = 382
- [x] Day 3 README is written and follows the README pattern used in Day 1 and Day 2

### Verification Result

The Day 3 implementation was compiled and executed successfully using:

```bash
cd /home/aarushb/Documents/file_server/file_server && gcc -Wall -Wextra -I include src/sched.c tests/mock_disk.c tests/test_sched.c -lpthread -o test_sched && ./test_sched
```

Output:

```text
scheduler fcfs test passed
scheduler fcfs textbook case passed
scheduler scan textbook case passed
scheduler cscan textbook case passed
```
