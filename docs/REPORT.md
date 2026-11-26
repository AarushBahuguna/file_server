---
title: "Multi-User File System with Concurrent Access"
subtitle: "Operating Systems Project Report"
author: "Arjun Bansal, Shubham Gupta, Shwetank Tiwari, Aarush Bahuguna"
date: "November 2026"
abstract: |
  Jaypee Institute of Information Technology, Noida.
  A client/server file system that lets several users work on one simulated disk at the same time.
  The report covers the storage layer, the client and IPC layer, per-file readers-writers locking with a
  buffer cache, deadlock detection, and disk-head scheduling.
---


# Introduction

## Problem statement

A file system that serves several users at once has to solve problems that a single-user file system
never meets. Two users may write the same file at the same moment, two users may each hold a file the other
one needs, and many small requests compete for one disk head. This project builds a complete miniature
system that faces all of these problems: a simulated disk with a hierarchical file system, a server that
handles many clients in parallel, locking that keeps files consistent, detection of deadlocks, and a choice
of disk-scheduling policies.

## Objectives

1. Store files and directories on a simulated disk and report fragmentation and free space.
2. Let many users use the file system at the same time through a command-line client.
3. Keep file contents correct under concurrent access with per-file readers-writers locks, without starving
   writers.
4. Detect and recover from deadlocks between users.
5. Reduce disk-head movement with FCFS, SCAN and C-SCAN scheduling and measure the difference, with and
   without a buffer cache.

## Team and modules

| Member | Module |
|---|---|
| Arjun Bansal | Simulated disk, allocation, directory tree, file operations, fragmentation report |
| Shubham Gupta | Client, IPC protocol, sessions, logging, statistics output, test scripts, build and Git |
| Shwetank Tiwari | Server, worker thread pool, readers-writers locks, starvation-aware policy, LRU cache with write-back |
| Aarush Bahuguna | Wait-for graph, deadlock detection and recovery, disk request queue, FCFS / SCAN / C-SCAN schedulers |

Each module was written against an agreed set of header files that were frozen on the first day, so every
member could build and test their part alone with stand-in code for the others.

## Structure of this report

Section 2 describes the overall architecture. Sections 3 to 6 describe the four modules in the order a request
meets them. Section 7 summarises testing and results, and Section 8 concludes.


# Architecture

## Overview

The system has three layers. Users type commands into a **client**. The client talks to one **server** through
named pipes (FIFOs). The server runs each request through a locking layer, a buffer cache and a disk scheduler
before the request reaches the simulated disk.

```text
  client (one per user)            server (one process, many threads)
 +---------------------+          +------------------------------------------+
 | CLI parser          |  FIFO    | dispatcher -> session table              |
 | chunking, timeouts  | -------> |     |                                    |
 | error messages      | <------- |     v                                    |
 +---------------------+  reply   | lock manager (readers-writers)           |
                          FIFO    |     |            \                       |
                                  |     |             deadlock detector      |
                                  |     v                                    |
                                  | file system (directories, allocation)    |
                                  |     |                                    |
                                  |     v                                    |
                                  | buffer cache (LRU, write-back)           |
                                  |     |                                    |
                                  |     v                                    |
                                  | disk scheduler (FCFS / SCAN / C-SCAN)    |
                                  |     |                                    |
                                  |     v                                    |
                                  | simulated disk                           |
                                  +------------------------------------------+
```

## Life of a request

1. The user types `write 3 0 hello`. The client parses and checks it.
2. The client builds a fixed-size request and writes it to the server FIFO.
3. A worker thread takes the request. The session table gives the user's current directory and the file
   behind descriptor 3.
4. The lock manager grants the file lock, or makes the request wait. A wait feeds the wait-for graph so a
   deadlock can be found.
5. The file system turns the file offset into disk blocks. Each block access goes through the cache, and a
   cache miss goes to the disk scheduler.
6. The server writes a response to the client's own reply FIFO, and the client prints the result.

## Module interfaces

The modules meet at a small number of functions that were fixed before coding started:
`disk_read` / `disk_write` and the `fs_*` calls (storage), `lock_acquire(file, mode, user)` and `lock_release`
(locking), `cache_get` / `cache_put` (cache) and `sched_submit` (disk scheduling). The wire format between
client and server is defined in `protocol.h` (Section 4).


# Storage Layer

TODO(Arjun): write this section. Cover: layout of the simulated disk (block size, superblock, free-block
bitmap), the allocation scheme and why it was chosen, the directory tree and path resolution, how
create/open/read/write/delete work, and how fragmentation and free space are computed (the `frag.*` and
`disk.*` statistics keys listed in `stats.h`). Add one figure of the disk layout.


# Client, IPC and Tooling

This section describes how users reach the server: the message format, the pipe-based transport, the
command-line client, per-user sessions, logging and statistics output.

## Message format

All messages are fixed-size C structures defined in `protocol.h`. A request (`request_t`) is 700 bytes and a
response (`response_t`) is 3,088 bytes. Both are below `PIPE_BUF` (4,096 bytes on Linux), and a compile-time
assertion stops the build if either grows past it.

A request carries a request id, the client's process id, the command, a file descriptor, an open mode, an
offset, a length, the user name, a path (up to 127 characters) and up to 512 bytes of data. A response carries
the same request id, a status code, a file descriptor, a length and up to 3,072 bytes of data.

| Command | Meaning |
|---|---|
| `LOGIN`, `LOGOUT` | start and end a session |
| `MKDIR`, `CD`, `LS` | directory operations |
| `CREATE`, `DELETE` | create or remove a file or an empty directory |
| `OPEN`, `CLOSE` | open a file for reading or writing and get a descriptor; release it |
| `READ`, `WRITE` | transfer bytes at an offset of an open file |
| `STATS` | request the statistics report |

There are ten status codes (success, no such file, already exists, no space, bad descriptor, permission,
invalid argument, deadlock victim, busy, internal error) and each has a readable message.

## Transport: named pipes

The server owns one well-known FIFO, `/tmp/fs_server_fifo`. Every client writes its requests there. Each client
also creates its own reply FIFO, `/tmp/fs_client_<pid>`, and the server answers on it, finding the name from the
process id inside the request.

Four decisions keep this simple and safe.

1. **Atomic writes.** POSIX guarantees that a `write` of at most `PIPE_BUF` bytes to a pipe is not interleaved
   with writes from other processes. Because every request is one fixed-size structure below that limit, many
   clients can write to the server FIFO at once with no locking at all.
2. **No blocking on open.** Opening a FIFO normally blocks until the other end opens it, and a reader sees
   end-of-file when the last writer closes. The server therefore opens its FIFO read-write, so it always has
   a writer and never sees end-of-file. Each client does the same with its reply FIFO. A client opens the server
   FIFO with `O_NONBLOCK` for writing, so a missing server gives an immediate error (`ENOENT` or `ENXIO`)
   instead of a hang.
3. **Timeout.** `ipc_call` waits for the reply with `poll`. The default is 30 seconds and can be changed with
   `FS_TIMEOUT_MS`. A server that is stuck, or a request that waits behind a lock for too long, becomes an
   error message (`ETIMEDOUT`), not a frozen terminal.
4. **Request-id matching.** After a timeout, the late reply may still arrive on the reply FIFO. `ipc_call`
   reads replies until it finds the one whose request id matches the request, and drops the others. A late
   reply is therefore never mistaken for the answer to the next request.

A signal such as Ctrl-C interrupts the wait (`EINTR`), so the client can close its files and log out cleanly.

## Command-line client

The client reads commands from the terminal or from standard input, so the same program is used by people and
by test scripts.

```text
mkdir <path>                    cd [path]                  ls [path]
create <path>                   delete <path>              open <path> [r|w]
close <fd>                      read <fd> <offset> <length>
write <fd> <offset> <text...>   stats [table|raw|csv]      help   exit
```

**Parsing.** A separate parser (`parse.c`) turns a line into a command structure and rejects mistakes before
anything is sent: missing or extra arguments, non-numeric numbers, bad open modes, and paths of 128 characters
or more. Each error says what was wrong and shows the correct usage.

**Large transfers.** The protocol limits one request to 512 bytes of data and one response to 3,072. The client
hides this. A `write` of 4,000 bytes is sent as eight requests and a `read` of 4,000 bytes as two, and the user
sees a single result.

**Error messages.** Errors name the command and the target, for example
`error: delete missing.txt: no such file or directory`. Some add a hint: a deadlock victim is told to repeat the
command, a full disk points to `stats`, a bad descriptor points to `open`.

**Cleanup.** The client remembers every descriptor it opened. When it exits, normally or after Ctrl-C, it closes
them and logs out, so no lock is left behind. It also refuses a `read`, `write` or `close` on a descriptor it does
not hold, without bothering the server.

## Sessions

The server keeps a session for every logged-in client, in a thread-safe table (64 sessions, 32 open files each)
keyed by the client's process id. A session stores the user name, the current directory and the open-file table.
Descriptors start at 3 and the lowest free number is reused.

The session module also turns relative paths into absolute ones, removing `.`, `..` and repeated slashes (`..` at
the root stays at the root). Logout returns the files that were still open so the dispatcher can release their
locks. A function that finds sessions whose client process no longer exists lets the server release the locks of
clients that crashed.

## Logging

All modules share one thread-safe logger. Each line holds a timestamp with milliseconds, a level, the process and
thread id, a component name and a message. The logger holds a mutex while it writes a whole line, so lines from
different threads never mix. One line per request records user, command, path, descriptor, offset, length,
status and latency:

```text
2026-11-26 02:29:05.405 [INFO ] [329/7fed54bdd740] req: id=2 user=alice cmd=MKDIR path=docs fd=0 off=0 len=0 -> OK (0.09 ms)
```

A script, `log_report.sh`, summarises such a log into a table of count, errors, average and maximum latency per
command and requests per user, or into CSV for graphs.

## Statistics output

The server answers `STATS` with plain text, one `key value` pair per line, for example `cache.hits 80`. Every
module owner adds their own numbers with three helper functions, and the client turns the text into a report with
sections for disk, fragmentation, cache, disk scheduler, locks and the server. The client also computes
disk usage, cache hit ratio and average head movement per request. `stats csv` prints the same data as CSV, ready for
graphs, and `stats raw` prints the text unchanged. A key that the client does not
know is shown under "Other", so nothing is lost.

## Performance of the IPC layer

To measure only the transport, an echo server (no file system) answered requests from 1 to 32 clients at the same
time. Each client sent 1,000 requests, one after the other, and waited for each reply.

| Clients | Total requests | Time (s) | Requests per second | Time per request seen by each client (ms) |
|---:|---:|---:|---:|---:|
| 1 | 1,000 | 0.008 | 125,069 | 0.008 |
| 2 | 2,000 | 0.014 | 143,572 | 0.014 |
| 4 | 4,000 | 0.024 | 164,024 | 0.024 |
| 8 | 8,000 | 0.046 | 173,352 | 0.046 |
| 16 | 16,000 | 0.094 | 170,684 | 0.094 |
| 32 | 32,000 | 0.199 | 161,208 | 0.199 |

A single client completes a full round trip (request out, reply back) in about 8 microseconds. Total throughput
rises until about 8 clients and then stays near 170,000 requests per second, because one server thread reads the
single server FIFO. The time each client waits grows in step with the number of clients, as expected for one
shared queue. The transport is therefore far faster than the disk operations behind it, and it is not the
bottleneck of the system. (The figures come from `make bench-ipc`, and the raw data is in
`docs/results/ipc_bench.csv`.)

## Problems found and fixed

- A late reply to a request that had timed out could be returned for the next request. Request-id matching fixed
  it, and a test now reproduces the situation.
- A client could wait for ever if the server stopped. The timeout fixed it.
- A client that quit without closing its files left their locks held. The client now closes its files, and the
  server session returns any that remain.
- A multi-client test script reported success even though a command had failed. It now checks the output of
  every client.
- The first fault tests passed for any failure. They now check the exact error code.

## Limitations

- Each client keeps one request in flight. If very many requests were waiting at once, the 64 KB pipe could fill
  and a write would fail with `EAGAIN`. A test with 300 clients at once did not reach this limit.
- A client killed with `SIGKILL` cannot remove its reply FIFO. The script `cleanup_fifos.sh` removes such files.
- The protocol has no "directory not empty" status, so deleting a non-empty directory returns "invalid argument".


# Server, Synchronization and Cache

TODO(Shwetank): write this section. Cover: the server loop and worker thread pool, the per-file
readers-writers lock and the policy that stops writers from starving, the LRU buffer cache with write-back
and dirty-block flushing, and the measured behaviour under contention (the `lock.*` and `cache.*`
statistics keys listed in `stats.h`).


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


# Testing and Results

## Test strategy

Every module has its own tests, and all of them run with one command, `make check`. Tests that need a server
use stand-in servers so that a module can be tested before the real server exists: an echo server (pure IPC),
a canned-answer server, and a small in-memory file system server that speaks the real protocol. The same
shell scripts then run unchanged against the real server by setting `SERVER_BIN`.

## Client and IPC tests

| Suite | What it checks | Size |
|---|---|---|
| `test_protocol` | message sizes stay below `PIPE_BUF`, name tables | unit |
| `test_parse` | every command, wrong arguments, over-long paths | unit |
| `test_session` | path normalisation, fd table, logout, dead clients, 8 racing threads | unit |
| `test_log` | 4 threads x 200 lines, no interleaving, level filter | unit |
| `test_stats` | key/value parser, table, CSV, derived values, legacy text | unit |
| echo test | 5 clients x 50 requests, replies verified byte for byte | end to end |
| CLI test | CLI against the canned server, client and server logs | 19 checks |
| ops scripts | files, directories, errors, statistics | 44 checks |
| IPC fault tests | server missing, wrong id, late reply, stale reply | 4 cases |
| `multi_rw.sh` | one writer and five readers on one file, torn-read detector | scenario |
| `stress.sh` | 16 clients x 20 iterations, 2560 requests, each verified | scenario |

The fault tests were checked by breaking the code on purpose. When `ipc_call` is changed to accept any reply, or
to wait without a timeout, the tests fail, so they do detect these faults.

## Results of other modules

TODO(Arjun, Shwetank): add your measured results (fragmentation, cache hit ratio, lock waits). Disk
scheduling and deadlock results are in Section 6.


# Conclusion

The project shows that a multi-user file system is mostly a coordination problem. Each module is small, but
correctness depends on how they meet: how requests are framed, how locks are ordered, and how long a client
may wait. Freezing the interfaces on day one, building stand-in servers early and giving every module its own
tests allowed four people to work in parallel without waiting for each other.

The most useful lessons were practical. Fixed-size messages below `PIPE_BUF` make concurrent writes to one
pipe safe without any lock. A request id on every message lets a client ignore a late reply instead of
mistaking it for the next answer. Timeouts turn a stuck server into a clear error message. A session table
that returns the open files at logout lets the server release locks of clients that crash.

## Future work

- Run the clients over sockets so that users can connect from other machines.
- Add a proper "directory not empty" status code to the protocol.
- Let the server push a message to a client whose request was chosen as a deadlock victim.
- Add file permissions per user.

TODO(all): after the final integration run, add one paragraph with the end-to-end results.


