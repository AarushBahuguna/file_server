# Multi-User File System: Deadlocks and Disk Scheduling

## Slide 1 — Problem

- Concurrent file access
- Lock ordering conflicts
- Disk head movement
- Need for deadlock recovery and scheduling comparisons

## Slide 2 — Architecture

```text
Cache -> Scheduler -> Disk
Lock requests -> Wait-for graph -> DFS detection -> Victim recovery
```

## Slide 3 — Wait-for Graph

- Node: client session
- Edge: waiter -> lock holder
- Cycle: every member waits for another member
- One 64×64 adjacency matrix with one graph mutex

## Slide 4 — Detection and Recovery

- DFS with white, gray, black states
- Back edge to gray node closes a cycle
- Select youngest session in cycle
- Mark aborted, remove edges, release locks, return -EDEADLK
- Buffered writes are discarded after abort

## Slide 5 — Disk Scheduling

- FCFS: arrival order
- SCAN: elevator, reverse at disk ends
- C-SCAN: one-direction sweep, return jump counted

## Slide 6 — Textbook Result

- Head 53
- Queue: 98, 183, 37, 122, 14, 124, 65, 67
- FCFS: 640
- SCAN: 236
- C-SCAN: 382

## Slide 7 — Race versus Locking

- No locks: concurrent writes can race
- Locking: writes serialize
- Consistent final state is guaranteed by lock discipline

## Slide 8 — Benchmark Method

- 1000 random requests
- Disk blocks 0–199
- Fixed seeds
- Cache on/off
- FCFS, SCAN, C-SCAN
- Five runs per configuration

## Slide 9 — Results

- SCAN and C-SCAN produce far less movement than FCFS on random workloads
- Cache hits reduce duplicate disk work
- C-SCAN has uniform sweep behavior with counted wrap movement

## Slide 10 — Deadlock Demo

1. Opposite lock order
2. Wait-for cycle appears
3. DFS detects cycle
4. Youngest victim aborted
5. Remaining session completes

## Slide 11 — Performance Demo

1. Run textbook workload
2. Verify exact movement totals
3. Run benchmark configurations
4. Show head movement and cache hit ratio

## Slide 12 — Conclusion

- Graph-based detection is inexpensive and explainable
- Recovery is explicit and measurable
- SCAN policies reduce random-workload head movement
- Unit and stress tests provide implementation confidence
