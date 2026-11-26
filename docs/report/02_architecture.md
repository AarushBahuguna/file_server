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
