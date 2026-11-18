# Aarush's Deadlock & Disk-Scheduling Module

## What This Is

This is the **deadlock detection** and **disk scheduling** component of a
multi-user file system project (OS course). It provides two modules:

1. **Disk Scheduler** (`include/sched.h` / `src/sched.c`)
   — queues disk I/O requests and services them using FCFS, SCAN, or C-SCAN.

2. **Wait-For Graph** (`include/waitfor.h` / `src/waitfor.c`)
   — tracks lock waits, detects cycles, and selects a victim for recovery.

---

## Day 4 Deliverables (Wait-For Graph)

Day 4 implements the wait-for graph data structure and cycle detection.

| File | Purpose |
|------|---------|
| `include/waitfor.h` | Wait-for graph API and Day 5 recovery helpers. |
| `src/waitfor.c` | 64×64 adjacency matrix, graph hooks, DFS cycle detection, and victim selection. |
| `tests/test_deadlock.c` | Unit tests for empty graphs, cycles, chains, stale edges, and recovery state. |

---

## Understanding the Graph

An edge `A -> B` means session A is waiting for a lock currently held by
session B. A cycle means no session in the cycle can progress.

| Function | Behavior |
|----------|----------|
| `wfg_on_wait(waiter, file_id, holders, n)` | Adds waiter-to-holder edges. |
| `wfg_on_grant(session)` | Removes all outgoing edges from the granted session. |
| `wfg_on_release(session)` | Removes all incoming edges to the released session. |
| `wfg_detect(start, cycle_path, cycle_len)` | Runs DFS to find a cycle and returns the youngest cycle member. |
| `wfg_dump()` | Prints all active graph edges. |

---

## Cycle Detection

The implementation uses DFS with white, gray, and black states:

- white: not visited
- gray: on the current DFS path
- black: fully explored

A back edge to a gray node identifies a cycle. The cycle is copied into
`cycle_path`, and the highest session ID in that cycle is selected as the
youngest victim.

On-request detection starts DFS at `start_session`. Periodic/full detection
passes `-1` and scans every graph node.

---

## Key Design Decisions

- **Adjacency matrix** — a fixed 64×64 matrix is simple and sufficient for
  this project's session limit.
- **Single graph mutex** — all matrix reads and writes are synchronized.
- **Waiter-to-holder edges** — matches the standard wait-for graph model.
- **No self edges** — a session waiting on itself is not represented as a
  separate graph cycle.
- **Youngest victim** — the largest session ID in the cycle is treated as
  the newest session and least-work-lost victim.
- **Lock ordering** — graph code does not call the lock manager while holding
  the graph mutex, and lock-manager hooks should not hold their lock mutex
  while blocking on graph access.

---

## Team Integration Points

| Call site | When to call |
|-----------|--------------|
| `wfg_on_wait(waiter, file_id, holders, n)` | Before a lock wait begins. |
| `wfg_on_grant(session)` | Immediately after a lock is granted. |
| `wfg_on_release(session)` | Immediately after a lock is released. |
| `wfg_detect(session, path, len)` | After adding a wait edge, or periodically. |

---

## How to Compile

```bash
cd /home/aarushb/Documents/file_server/file_server
gcc -Wall -Wextra -I include src/waitfor.c tests/test_deadlock.c \
    -lpthread -o test_deadlock
./test_deadlock
```

---

## Definition of Done for Day 4

- [x] 64×64 graph initialized and destroyed cleanly
- [x] Wait, grant, and release hooks maintain graph edges
- [x] Shared-holder waits add one edge per holder
- [x] Self edges are skipped
- [x] DFS detects two-session and three-session cycles
- [x] Long chains do not produce false deadlock results
- [x] Grant and release remove stale edges

### Verification Result

The Day 4 graph implementation and tests pass through the deadlock test binary.
