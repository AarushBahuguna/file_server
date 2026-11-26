# Demo Likely Questions and Answers

## Why does an edge point from waiter to holder?

That direction expresses the resource dependency. A cycle proves that every
session in the cycle is blocked by another session in the cycle.

## Why is the youngest victim selected?

The highest session ID is used as the newest session, so least work is assumed
lost. The policy is isolated and can later be replaced by priority or work-count.

## What is the difference between on-request and periodic detection?

On-request starts DFS only from the newly waiting session and catches the cycle
that an added edge may have formed. Periodic detection scans every node roughly
once per second and can catch missed or stale graph updates.

## Why does C-SCAN count the return jump?

The return is real head movement. Counting 199 to 0 plus request movement keeps
the textbook result consistent and avoids understating physical travel.

## Why must SCAN stop at 0 and 199?

Those are the real disk boundaries in the textbook. The elevator must travel to
the boundary before reversing, or the movement total would be incorrect.

## Why do batching or seek delay matter?

If one request arrives at a time, the queue is usually empty and FCFS, SCAN, and
C-SCAN make identical choices. Multiple queued requests are required to expose
policy differences.

## How does the cache affect the benchmark?

Cache hits avoid new disk requests and reduce duplicate work. The benchmark
reports cache hit ratio alongside head movement and latency.

## How do stale edges cause false positives?

A waiter edge that remains after grant or a holder edge that remains after release
creates dependencies that no longer exist. `wfg_on_grant` and `wfg_on_release`
remove the corresponding stale edges.

## How are shared locks handled?

A waiting writer receives an edge to every current reader holder. Self edges are
skipped. Lock upgrade behavior should be decided explicitly to avoid accidental
self-deadlocks.

## How is recovery safe for multiple-lock operations?

Writes are buffered until all needed locks are held. If the session is aborted,
its buffer is discarded instead of requiring an undo operation for every write.

## Why can no-lock races corrupt data?

Two writers can overlap, and the final content depends on the interleaving. With
locking, the write critical section is serialized and final state remains
internally consistent.

## What mutex-ordering rule prevents graph/lock deadlocks?

Do not hold the graph mutex while calling lock-manager code. Do not hold the
lock-manager mutex while blocking in graph code. This prevents reverse lock
ordering and keeps graph hooks independent of lock-manager waits.
