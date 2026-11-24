# Deadlock Detection and Disk Scheduling

## Wait-For Graph

A wait-for graph node represents a client session. A directed edge
`waiter -> holder` means the waiter is blocked on a lock held by the holder.
For example:

```text
Session 0 -> Session 1 -> Session 0
```

This 2-cycle is a deadlock. The graph is a 64x64 adjacency matrix protected by
one mutex. `wfg_on_wait` adds edges, `wfg_on_grant` removes outgoing edges, and
`wfg_on_release` removes incoming edges so stale waits do not cause false alarms.

## Detection and Recovery

Detection uses depth-first search with white, gray, and black states. An edge to
a gray node closes a cycle and provides the cycle path. The detector selects the
youngest session in the cycle, marks it aborted, removes all its graph edges,
wakes the abortable lock wait, releases its locks, and returns `-EDEADLK`.

Buffered writes allow recovery to discard all uncommitted work instead of undoing
individual operations.

## Race versus Locked Writes

Two writers without locking can both read or update the same file and race to the
last write, producing an interleaving-dependent final value. Locking serializes
access so each successful write is based on consistent shared state.

## Disk-Scheduling Comparison

| Policy | Disk behavior | Day 3 textbook movement |
|--------|---------------|--------------------------|
| FCFS | Arrival order | 640 |
| SCAN | Sweep to an end, reverse, sweep back | 236 |
| C-SCAN | Sweep in one direction, count wrap to start | 382 |

The elevator policies reduce travel by grouping nearby requests. C-SCAN is more
uniform across the disk because it does not serve requests while returning to the
start. FCFS remains the least complex policy but is sensitive to random arrival
order.

## Cache and Batch Effects

The benchmark compares cache hit ratios and latency with cache enabled and disabled.
Hits avoid disk scheduling work and generally improve latency. The batch window or
simulated seek delay is required in practice because an empty queue makes all
scheduling policies behave like FCFS for a one-request-at-a-time workload.

## Reproducibility

The benchmark uses 1000 fixed-seed requests, disk blocks 0–199, initial head 53,
three policies, cache-on/cache-off modes, and five runs per configuration. Results
are written to `docs/results/bench_results.csv`.
