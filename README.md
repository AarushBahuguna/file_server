# Aarush's Deadlock & Disk-Scheduling Module

## What This Is

This is the **deadlock detection** and **disk scheduling** component of a
multi-user file system project (OS course).  It provides two modules:

1. **Disk Scheduler** (`include/sched.h` / `src/sched.c`)
   — queues disk I/O requests and services them using one of three
   policies: FCFS, SCAN, or C-SCAN.

2. **Wait-For Graph** (`include/waitfor.h` / `src/waitfor.c`)
   — tracks which sessions are waiting for which locks, detects
   deadlocks (cycles), and picks a victim to abort.

---

## Day 1 Deliverables (Contracts)

Day 1 is about writing the **header files only** — the "contracts" that
the rest of the team codes against.  No implementation yet.

| File                | Purpose |
|---------------------|---------|
| `include/sched.h`   | Defines `struct disk_req`, `struct sched_stats`, and the scheduler API functions (`sched_init`, `sched_submit`, `sched_set_policy`, `sched_stats`, `sched_shutdown`). |
| `include/waitfor.h` | Defines the four wait-for graph hooks (`wfg_on_wait`, `wfg_on_grant`, `wfg_on_release`, `wfg_detect`) plus `wfg_dump`, `wfg_init`, and `wfg_destroy`. |

---

## File Structure

```
os-project/
├── include/
│   ├── sched.h            ← scheduler API (Day 1) ✅
│   └── waitfor.h          ← wait-for graph API (Day 1) ✅
├── src/
│   ├── sched.c            ← scheduler implementation (Day 2-3)
│   ├── waitfor.c          ← deadlock detector (Day 4-5)
│   └── bench.c            ← benchmarking module (Day 8)
├── tests/
│   ├── mock_disk.c        ← fake disk for unit testing (Day 2)
│   ├── test_sched.c       ← scheduler tests (Day 3, 6)
│   └── test_deadlock.c    ← deadlock tests (Day 5, 6)
├── scripts/               ← demo & bench scripts (Day 8)
├── docs/                  ← report and results (Day 9)
└── README.md              ← this file
```

---

## Understanding the Code

### sched.h — Disk Scheduler API

| Item | What It Is |
|------|------------|
| `SCHED_FCFS`, `SCHED_SCAN`, `SCHED_CSCAN` | Constants (0, 1, 2) identifying the three scheduling policies. |
| `struct disk_req` | Represents one disk I/O request. The client fills in `block`, `is_write`, and `buf`, then waits on the `done` semaphore. The scheduler sets `status` and posts `done` when finished. |
| `struct sched_stats` | Performance counters: total head movement, requests served, total wait time. |
| `sched_init()` | Starts the scheduler thread and initialises the queue. Call once at startup. |
| `sched_submit(r)` | Adds request `r` to the queue, wakes the scheduler, and blocks until serviced. Returns the status code. |
| `sched_set_policy(p)` | Switches the active policy (FCFS / SCAN / C-SCAN). Takes effect on the next pick. |
| `sched_stats(out)` | Copies current counters into `*out`. |
| `sched_shutdown()` | Stops the scheduler thread gracefully. |

### waitfor.h — Wait-For Graph API

| Item | What It Is |
|------|------------|
| `MAX_SESSIONS` | Maximum concurrent sessions (64). The graph is a 64×64 adjacency matrix. |
| `wfg_init()` | Initialises the graph (clears matrix, creates mutex). Call once at startup. |
| `wfg_on_wait(waiter, file, holders, n)` | Adds edges from `waiter` to each `holder`. Called just before a session blocks on a lock. |
| `wfg_on_grant(session)` | Removes all outgoing edges from `session`. Called after a lock is granted. |
| `wfg_on_release(session)` | Removes all incoming edges to `session`. Called after a lock is released. |
| `wfg_detect(start, path, len)` | Runs DFS to find a cycle. Returns the victim's session ID, or -1 if no deadlock. |
| `wfg_dump()` | Prints all edges to stdout (for debugging). |
| `wfg_destroy()` | Cleans up. Call at shutdown. |

---

## How to Compile (once implementations exist)

Right now the headers have no implementation, so there is nothing to run.
Once `src/sched.c` and `tests/mock_disk.c` are written (Day 2), you can
compile and test like this:

```bash
# Compile the scheduler test (Day 2+)
gcc -Wall -Wextra -I include \
    src/sched.c tests/mock_disk.c tests/test_sched.c \
    -lpthread -o test_sched

./test_sched

# Compile the deadlock test (Day 5+)
gcc -Wall -Wextra -I include \
    src/waitfor.c tests/test_deadlock.c \
    -lpthread -o test_deadlock

./test_deadlock
```

### Verify headers compile today (Day 1)

```bash
gcc -fsyntax-only -Wall -Wextra -I include include/sched.h
gcc -fsyntax-only -Wall -Wextra -I include include/waitfor.h
```

Both should produce **zero warnings and zero errors**.

---

## Key Design Decisions

- **Pure `pick_next` function** — The "which request is next" logic will
  be a plain function with no locks, so it can be tested by hand against
  textbook numbers.
- **Adjacency matrix for the graph** — Simple, fixed-size, fast for 64
  sessions.  One mutex protects it.
- **Edge = waiter → holder** — Standard wait-for graph definition,
  matching the textbook.
- **Youngest-victim policy** — When a deadlock is found, abort the
  newest session (least work lost).
- **Abortable wait** — The lock manager must use a timed wait or a
  condition variable with an abort flag, so the detector can wake and
  kill the victim.

---

## Team Integration Points

| Hook call site (in rwlock.c) | When to call |
|------------------------------|--------------|
| `wfg_on_wait(waiter, file, holders, n)` | **Before** the session blocks on the lock |
| `wfg_on_grant(session)` | **After** the lock is granted to a previously-blocked session |
| `wfg_on_release(session)` | **After** the session releases its lock |

| Scheduler call site (in cache.c) | When to call |
|-----------------------------------|--------------|
| `sched_submit(&req)` | On a cache miss or dirty eviction — the request goes to the disk scheduler |

---

## What's Next (Day 2)

- `tests/mock_disk.c` — fake `disk_raw_read` / `disk_raw_write`
- `src/sched.c` — request queue, scheduler thread loop, FCFS policy
- First passing test: 20 requests served in arrival order
