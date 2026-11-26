# Aarush's Deadlock & Disk-Scheduling Module

## Day 7 Deliverables (Integration)

Day 7 verifies the scheduler and wait-for graph independently before full team integration.

| File | Purpose |
|------|---------|
| `tests/test_integration.c` | Cache-miss -> scheduler -> mock-disk integration check. |
| `src/sched.c` | Disk scheduling implementation used by cache misses. |
| `src/waitfor.c` | Deadlock graph implementation used by lock wait hooks. |

## Integration Flow

```text
cache miss -> sched_submit -> scheduler thread -> disk_raw_read/write
                                                 -> update head movement
lock wait -> wfg_on_wait -> detector -> wfg_recover_victim -> -EDEADLK
lock grant/release -> remove stale wait-for graph edges
```

## Verification

```bash
gcc -Wall -Wextra -I include src/sched.c tests/mock_disk.c tests/test_integration.c -lpthread -o test_integration
./test_integration
```

The integration test verifies that a cache miss services block 98, issues one mock read, and changes total head movement from 53 to 45 cylinders of movement.

## Definition of Done

- [x] Cache-miss request submits to the scheduler
- [x] Scheduler calls the mock disk
- [x] Head-movement counter changes
- [x] Lock graph hooks have documented integration call sites
- [x] Unit suites remain independently executable
