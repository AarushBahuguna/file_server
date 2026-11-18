# Aarush's Deadlock & Disk-Scheduling Module

## What This Is

This is the **deadlock detection** and **disk scheduling** component of a
multi-user file system project (OS course). Day 5 adds detection counters,
victim recovery state, and unit-test validation to the wait-for graph from
Day 4.

---

## Day 5 Deliverables (Detection and Recovery)

Day 5 completes deadlock detection behavior and makes the recovery path
independently testable.

| File | Purpose |
|------|---------|
| `include/waitfor.h` | Adds recovery, abort-state, and statistics APIs. |
| `src/waitfor.c` | Tracks detection/deadlock counters and marks/releases victims. |
| `tests/test_deadlock.c` | Tests cycles, chains, stale edges, victim selection, and abort state. |

---

## Detection Modes

### On-request detection

A lock wait calls:

```c
wfg_detect(waiter, cycle_path, &cycle_len);
```

The DFS begins at the session that formed the new wait edge, allowing a
fast check for the cycle that was just created.

### Periodic detection

A background detector calls:

```c
wfg_detect(-1, cycle_path, &cycle_len);
```

A full scan is scheduled only once per detection interval. The graph scan
itself remains behind the graph mutex and does not call lock-manager code.

---

## Recovery Sequence

The working plan defines this recovery order:

1. Detect the cycle and record its session path.
2. Select the youngest session in the cycle.
3. Mark the victim aborted.
4. Wake or abort the victim through the lock manager's abort mechanism.
5. Release all locks held by the victim, removing its incoming and outgoing edges.
6. Return `-EDEADLK` to the victim's client.

The standalone implementation provides:

- `wfg_recover_victim(victim)`
- `wfg_is_aborted(session)`
- `wfg_reset_abort(session)`
- `wfg_stats(&detections, &deadlocks)`

`wfg_recover_victim()` marks the victim, removes all graph edges associated
with it, and returns `-EDEADLK`. It can then be wired to an actual lock
manager abort flag during system integration.

---

## Unit Test Coverage

The Day 5 tests cover:

- empty graph
- 2-session cycle
- 3-session cycle
- chain with no cycle
- stale outgoing edges after grant
- stale incoming edges after release
- duplicate and self-edge handling
- youngest-session victim selection
- abort and recovery state
- detection statistics

---

## Key Design Decisions

- **DFS back-edge detection** — gray nodes identify an active DFS path.
- **Youngest victim** — maximizes retained work and is easy to explain.
- **Graph-only recovery stub** — the module marks state and removes edges;
  the real lock manager performs the physical wake and lock release.
- **Buffered writes remain integration responsibility** — a multi-lock
  operation should discard its write buffer after `-EDEADLK`.
- **Deadlock and detection counters** — counters help validate on-request and
  periodic detector modes later.

---

## How to Compile

```bash
cd /home/aarushb/Documents/file_server/file_server
gcc -Wall -Wextra -I include src/waitfor.c tests/test_deadlock.c \
    -lpthread -o test_deadlock
./test_deadlock
```

Run repeatedly to check stability:

```bash
for i in $(seq 1 100); do ./test_deadlock >/dev/null || exit 1; done
echo "deadlock tests passed 100 runs"
```

---

## Integration Notes

The team lock manager should:

- call `wfg_on_wait()` while holding its lock state, before blocking;
- call `wfg_on_grant()` after a wait is granted;
- call `wfg_on_release()` after a lock is released;
- check an abort flag immediately after wakeup;
- release all victim locks before returning `-EDEADLK`.

The graph mutex must never be held while calling the lock manager, and the
lock-manager mutex must never be held while blocking on the graph mutex.

---

## Definition of Done for Day 5

- [x] DFS detects and records cycles
- [x] Youngest cycle member is selected as victim
- [x] Victim is marked aborted
- [x] Recovery removes incoming and outgoing victim edges
- [x] Recovery returns `-EDEADLK`
- [x] Detection and deadlock counters are available
- [x] Two-cycle, three-cycle, and no-cycle tests are present
- [x] Grant/release stale-edge tests are present

### Verification Result

The complete Day 4–5 implementation is verified with `test_deadlock` and
100 repeated executions once the binary passes cleanly.
