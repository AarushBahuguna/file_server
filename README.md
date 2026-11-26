# Multi-User File System with Concurrent Access

A Linux/C educational project implementing a concurrent file system with:

- Arjun's simulated disk and indexed file system
- Shwetank's server worker pool, reader-writer locks, and LRU cache
- Aarush's disk scheduling and wait-for graph deadlock detector
- Shubham's fixed-size protocol, FIFO IPC, CLI, sessions, logging, and statistics

## Project Layout

```text
include/   public headers
src/       integrated implementation
  common/  Shubham protocol, IPC, session, log, and stats
  client/  Shubham CLI and parser
tests/     member and integration tests
scripts/   demos and operation scripts
docs/      reports and benchmark results
readmeS/   existing Aarush, Arjun, Shwetank, and Shubham README files
read/      text summaries of work completed by each member
other_members/  original uploaded team folders (reference only)
```

## Main Components

### Storage — Arjun

- 2048-block disk, 512-byte blocks
- 128-inode layout
- 10 direct plus 1 indirect allocation
- nested directories and path resolution
- persistence through `disk.img`
- file reporting and `fs_check` consistency validation

### Server and Synchronization — Shwetank

- FIFO listener and bounded worker pool
- reader-writer lock table
- fair/reader/writer lock policies
- 64-entry LRU cache
- dirty write-back and cache statistics
- metadata/cache/scheduler lock-order design

### Scheduling and Deadlock — Aarush

- FCFS, SCAN, C-SCAN disk scheduling
- 64×64 wait-for graph
- DFS cycle detection
- youngest-session victim policy
- abort/recovery support
- textbook and random workload tests

### Protocol and Client — Shubham

- fixed-size `request_t` and `response_t`
- server and per-PID reply FIFOs
- session/cwd/open-FD table
- interactive CLI and script input
- thread-safe logging
- table/raw/CSV statistics formatting

## Build

```bash
mkdir -p logs

gcc -std=gnu11 -Wall -Wextra -Iinclude -Isrc \
  src/disk.c src/alloc.c src/dir.c src/fs.c \
  src/sched.c src/waitfor.c src/threadpool.c src/dispatcher.c \
  src/rwlock.c src/cache.c src/server.c src/server_tcp.c \
  src/common/protocol.c src/common/ipc.c src/common/ipc_tcp.c src/common/log.c \
  src/common/session.c src/common/stats.c -lpthread -o server

gcc -std=gnu11 -Wall -Wextra -Iinclude -Isrc \
  src/common/protocol.c src/common/ipc.c src/common/ipc_tcp.c src/common/log.c \
  src/common/session.c src/common/stats.c \
  src/client/parse.c src/client/main.c -lpthread -o client
```

## Run

### Local FIFO mode

Terminal 1:

```bash
./server
```

Terminal 2:

```bash
./client alice
```

### Multi-laptop TCP mode

On the server laptop:

```bash
hostname -I
export FS_TRANSPORT=tcp
export FS_SERVER_PORT=9000
./server
```

Open TCP port 9000 in the server laptop firewall, then run on each client laptop:

```bash
export FS_TRANSPORT=tcp
export FS_SERVER_HOST=<server-laptop-IP>
export FS_SERVER_PORT=9000
./client alice
```

Each client receives a stable network session identity and may connect from a different laptop.

Interactive commands include `mkdir`, `cd`, `ls`, `create`, `open`, `close`, `read`,
`write`, `delete`, `stats`, `help`, and `exit`.

Pipe commands for a script-style run:

```bash
printf 'create demo.txt\nopen demo.txt w\nwrite 3 0 hello\nclose 3\nexit\n' | ./client alice
```

## Tests

```bash
gcc -std=gnu11 -Wall -Wextra -Iinclude -Isrc tests/test_protocol.c src/common/protocol.c -o test_protocol
./test_protocol

gcc -std=gnu11 -Wall -Wextra -Iinclude -Isrc tests/test_session.c src/common/session.c src/common/protocol.c -lpthread -o test_session
./test_session

gcc -std=gnu11 -Wall -Wextra -Iinclude -Isrc tests/test_stats.c src/common/stats.c -o test_stats
./test_stats

gcc -std=gnu11 -Wall -Wextra -Iinclude -Isrc src/disk.c src/alloc.c src/dir.c src/fs.c tests/test_fs.c -lpthread -o test_fs
./test_fs
```

## Documentation

- Existing member/day READMEs: `readmeS/`
- Per-member work summaries: `read/`
- Detailed run instructions: `HOW_TO_RUN.txt`
- Original Arjun/Shwetank/Shubham uploads: `other_members/`

## Integration Status

- Arjun storage is integrated in `src/disk.c`, `src/alloc.c`, `src/dir.c`, and `src/fs.c`.
- Shwetank server, cache, and lock code is integrated in `src/server.c`, `src/threadpool.c`, `src/dispatcher.c`, `src/rwlock.c`, and `src/cache.c`.
- Aarush scheduler and wait-for graph code remains in `src/sched.c` and `src/waitfor.c`.
- Shubham protocol, IPC, session, logger, statistics, parser, and CLI code are integrated.
- The dispatcher maps OS PIDs to bounded wait-for graph session IDs and tracks logical open FDs separately from Arjun storage descriptors.
