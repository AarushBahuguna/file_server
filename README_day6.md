# Aarush's Deadlock & Disk-Scheduling Module

## What This Is

This is the **deadlock detection** and **disk scheduling** component of a
multi-user file system project (OS course). Day 6 is the unit-test checkpoint:
both modules must pass textbook, random, cycle, chain, and repeated-run tests
before team integration begins.

---

## Day 6 Deliverables (Unit-Test Checkpoint)

Day 6 hardens the scheduler and wait-for graph tests completed in Days 2–5.

| File | Purpose |
|------|---------|
| `tests/test_sched.c` | Textbook policy totals and fixed-seed random exactly-once workload tests. |
| `tests/mock_disk.c` | Records disk operations and per-block service counts. |
| `tests/test_deadlock.c` | Cycle, chain, stale-edge, and opposite-thread-order deadlock tests. |
| `src/sched.c` | Scheduler implementation exercised by the checkpoint suite. |
| `src/waitfor.c` | Graph and recovery implementation exercised by the checkpoint suite. |

---

## Scheduler Tests

### Textbook example

The test uses head = 53 and queue:

```text
98 183 37 122 14 124 65 67
```

Expected totals:

- FCFS = 640
- SCAN = 236
- C-SCAN = 382

### Random workload

The random test uses a fixed seed and submits 200 requests for each policy.
It verifies:

- every request receives a successful completion status;
- the scheduler reports exactly 200 requests served;
- the mock disk receives exactly 200 read/write operations;
- no request is lost or completed by an unrelated request.

Fixed seeds make failures reproducible and prevent lucky timing or scheduling
from hiding implementation defects.

---

## Deadlock Tests

The deadlock suite checks:

- empty graph behavior;
- two-session cycle detection;
- three-session cycle detection;
- a long wait chain with no false positive;
- stale outgoing edges after grant;
- stale incoming edges after release;
- duplicate holders and self-edge handling;
- youngest-victim selection;
- victim abort and recovery state;
- opposite-thread lock ordering with forced sleeps;
- detection and deadlock counters.

The opposite-order test creates wait edges from both thread interleavings and
then verifies that the full graph DFS finds the resulting two-session cycle.

---

## How to Compile

```bash
cd /home/aarushb/Documents/file_server/file_server

gcc -Wall -Wextra -I include \
    src/sched.c tests/mock_disk.c tests/test_sched.c \
    -lpthread -o test_sched

gcc -Wall -Wextra -I include \
    src/waitfor.c tests/test_deadlock.c \
    -lpthread -o test_deadlock
```

## How to Run

```bash
./test_sched
./test_deadlock
```

## 100-Run Stress Verification

```bash
for i in $(seq 1 100); do
    ./test_sched >/dev/null || exit 1
    ./test_deadlock >/dev/null || exit 1
done

echo "Day 6 scheduler and deadlock suites passed 100 runs"
```

---

## Key Design Decisions

- **Known-answer tests first** — policy totals are compared with textbook values.
- **Fixed random seeds** — random workloads stay reproducible across runs.
- **Exactly-once accounting** — successful status, scheduler counters, and mock
  disk operation totals must agree.
- **No graph leakage** — each graph test initializes and destroys its graph.
- **Forced timing only for synchronization coverage** — the threaded deadlock test
  uses sleeps to establish opposite lock-order interleavings, but cycle and
  chain correctness is decided by the graph implementation itself.
- **Immediate failure** — all 100 repeated runs stop at the first mismatch.

---

## Definition of Done for Day 6

- [x] Textbook scheduler totals pass
- [x] FCFS random workload passes
- [x] SCAN random workload passes
- [x] C-SCAN random workload passes
- [x] Every random request completes exactly once
- [x] Two-session cycle passes
- [x] Three-session cycle passes
- [x] Wait chain produces no false cycle
- [x] Opposite-thread lock ordering produces a cycle
- [x] Both complete suites pass 100 repetitions

### Verification Result

Once the binaries pass, compile-time and runtime verification should report:

```text
scheduler fcfs test passed
scheduler fcfs textbook case passed
scheduler scan textbook case passed
scheduler cscan textbook case passed
scheduler fcfs random workload passed
scheduler scan random workload passed
scheduler cscan random workload passed
deadlock graph tests passed
2-cycle, 3-cycle, chain, grant/release, self-edge, and stats checks passed
Day 6 scheduler and deadlock suites passed 100 runs
```
