# Shubham Client, IPC, and Protocol — Integrated README

## Overview

Shubham's fork was created after the earlier project work and contains a complete
protocol, client, IPC, session, logging, statistics, test, and script implementation.
It is integrated into the root project while preserving the already integrated Arjun,
Shwetank, and Aarush implementations.

Forked duplicate files for Aarush's scheduler/wait-for graph were intentionally not
copied. The root versions remain the authoritative implementations.

## Integrated files

| File | Purpose |
|------|---------|
| `include/protocol.h` | Fixed-size request/response protocol and status codes. |
| `include/ipc.h` | FIFO server/client IPC API. |
| `include/session.h` | Thread-safe session, cwd, and open-FD table API. |
| `include/log.h` | Thread-safe server/client logger. |
| `include/parse.h` | Interactive and script command parser. |
| `include/stats.h` | Server statistics key/value format and client rendering. |
| `src/common/protocol.c` | Command/status name conversion. |
| `src/common/ipc.c` | Fixed-size FIFO transport, timeout, and request-ID matching. |
| `src/common/session.c` | Per-PID sessions and absolute path resolution. |
| `src/common/log.c` | Thread-safe timestamped logging. |
| `src/common/stats.c` | Statistics parsing/table/raw/CSV formatting. |
| `src/client/parse.c` | CLI command parser. |
| `src/client/main.c` | Interactive/script client. |

## Protocol

The frozen IPC contract uses:

- Server FIFO: `/tmp/fs_server_fifo`
- Reply FIFO: `/tmp/fs_client_<pid>`
- Fixed-size `request_t` and `response_t` structures
- Atomic request writes and request-ID response matching
- Timeout and stale-reply handling
- Session login/logout keyed by client PID

Commands include login, logout, mkdir, cd, ls, create, open, close, read, write,
delete, and stats.

## Fork-time integration

Shubham's branch used a fixed-size binary-style protocol, replacing the earlier
text-request server boundary. The integrated root therefore:

- uses `request_t` from `protocol.h` in the thread pool
- uses `ipc_recv_request` and `ipc_send_response` in the server
- uses Shubham's session module for client PID, cwd, and logical FDs
- keeps Arjun's filesystem operations behind the dispatcher
- keeps Shwetank's lock/cache/server synchronization modules behind the dispatcher
- keeps Aarush's scheduler and wait-for graph in `src/sched.c` and `src/waitfor.c`

## Server integration flow

```text
client
  -> fixed-size request_t
  -> /tmp/fs_server_fifo
  -> bounded thread pool
  -> Shubham session/module lookup
  -> Shwetank lock/cache
  -> Arjun fs_* operation
  -> Aarush sched_submit for cache misses/writes
  -> Arjun disk_raw_* disk access
  -> fixed-size response_t
  -> /tmp/fs_client_<pid>
```

## Build

```bash
cd /home/aarushb/Documents/file_server/file_server

gcc -std=gnu11 -Wall -Wextra -Iinclude -Isrc \
    src/common/protocol.c src/common/ipc.c src/common/log.c \
    src/common/session.c src/common/stats.c \
    src/client/parse.c src/client/main.c -lpthread -o client

gcc -std=gnu11 -Wall -Wextra -Iinclude -Isrc \
    src/disk.c src/alloc.c src/dir.c src/fs.c \
    src/sched.c src/waitfor.c src/threadpool.c src/dispatcher.c \
    src/rwlock.c src/cache.c src/server.c \
    src/common/protocol.c src/common/ipc.c src/common/log.c \
    src/common/session.c src/common/stats.c -lpthread -o server
```

## Run

Terminal 1:

```bash
mkdir -p logs
./server
```

Terminal 2:

```bash
./client alice
```

## Unit tests

```bash
./test_protocol
./test_session
./test_stats
./test_log
```

## Scripts

Useful integrated scripts include:

- `scripts/cleanup_fifos.sh`
- `scripts/log_report.sh`
- `scripts/multi_client.sh`
- `scripts/run_ops.sh`
- `scripts/ops_basic.sh`
- `scripts/ops_errors.sh`
- `scripts/ops_stats.sh`

## Known integration items

- The real dispatcher maps Shubham logical FDs to Arjun open-file descriptors.
- Session logout should close and release every open file and lock before replying.
- Crashed clients should have reply/session cleanup performed by `session_find_dead()` and
  FIFO cleanup.
- The real dispatcher should acquire file locks according to command mode before each
  filesystem operation and release them after completion.
- Script and stress tests should target the real `server` binary once it is validated.
