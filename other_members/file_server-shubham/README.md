# Multi-User File System with Concurrent Access

Operating Systems project, Jaypee Institute of Information Technology, Noida.

Several users work on one simulated disk at the same time. Users run a command-line **client**; a **server**
handles all requests in parallel, keeps files consistent with per-file readers-writers locks and an LRU buffer
cache, detects deadlocks between users, and schedules disk-head movement (FCFS, SCAN, C-SCAN).

## Team

| Member | Module |
|---|---|
| Arjun Bansal | simulated disk, allocation, directories, file operations, fragmentation report |
| Shubham Gupta | client, IPC protocol, sessions, logging, statistics output, test scripts, build |
| Shwetank Tiwari | server, thread pool, readers-writers locks, LRU cache with write-back |
| Aarush Bahuguna | wait-for graph, deadlock detection, disk queue and schedulers |

## Status

| Part | State |
|---|---|
| Message format, FIFO IPC with timeout and request-id matching | done, tested |
| CLI client (parser, chunking, error messages, cleanup on exit) | done, tested |
| Sessions, logging, statistics format and report | done, tested |
| Disk scheduler, wait-for graph (`src/sched.c`, `src/waitfor.c`) | in the repository, own tests |
| Server, locks, cache, file system | integration in progress (see `docs/bug_log.md`) |

Until the real server is in `bin/server`, every client-side test runs against `bin/fake_fs_server`, a small
in-memory file system server that speaks the real protocol.

## Build

Needs Linux (or WSL), `gcc` and `make`.

```bash
make            # builds bin/client (and bin/server once it exists)
make check      # builds and runs every test (about 10 seconds)
```

## Try it

Terminal 1:

```bash
make bin/fake_fs_server && ./bin/fake_fs_server      # or ./bin/server for the real one
```

Terminal 2:

```bash
./bin/client alice
alice:/> mkdir /docs
alice:/> cd /docs
alice:/docs> create a.txt
alice:/docs> open a.txt w
fd = 3
alice:/docs> write 3 0 hello world
wrote 11 bytes
alice:/docs> close 3
alice:/docs> ls
a.txt
alice:/docs> stats
```

Open more terminals and start `./bin/client bob`, `./bin/client carol`, and so on. All of them share the same
disk.

### Commands

| Command | Meaning |
|---|---|
| `mkdir <path>` | create a directory |
| `cd [path]` | change directory (no path = `/`) |
| `ls [path]` | list a directory (directories end with `/`) |
| `create <path>` | create an empty file (`touch`) |
| `open <path> [r\|w]` | open a file and print its descriptor (default `r`) |
| `close <fd>` | close a file |
| `read <fd> <offset> <length>` | read bytes |
| `write <fd> <offset> <text...>` | write text |
| `delete <path>` | delete a file or an empty directory (`rm`) |
| `stats [table\|raw\|csv]` | disk, fragmentation, cache, scheduler and lock statistics |
| `help`, `exit` | help; log out and quit |

Large reads and writes are split into protocol-sized pieces by the client. Commands can also be piped in:
`printf 'ls\nexit\n' | ./bin/client alice`.

### Settings

| Variable | Effect |
|---|---|
| `FS_CLIENT_LOG=<file>` | the client logs every request with its latency |
| `FS_TIMEOUT_MS=<ms>` | how long the client waits for a reply (default 30000) |
| `SERVER_BIN=<path>` | which server the test scripts start (default `bin/fake_fs_server`) |

## Tests and scripts

| Command | What it does |
|---|---|
| `make check` | unit tests, echo test, CLI test, ops scripts, IPC fault tests, concurrency and stress scenarios |
| `make demo` | narrated demo for the presentation (`DEMO_PAUSE=1` waits for Enter between steps) |
| `make multi-rw` | one writer and several readers on one file; detects torn reads |
| `make stress` | many clients create, write, read back and delete files, every result verified |
| `make bench-ipc` | IPC throughput for 1 to 32 clients, saved to `docs/results/ipc_bench.csv` |
| `make integration` | run the client-side test set against the real server (Day 7) |
| `make report` | build `docs/REPORT.docx` from `docs/report/*.md` |
| `scripts/log_report.sh <log> [--csv]` | summarise a request log |
| `scripts/cleanup_fifos.sh` | remove reply FIFOs of killed clients |

Against the real server: `SERVER_BIN=./bin/server EXPECT_LOCKS=1 ./scripts/run_ops.sh` (and the same for
`multi_rw.sh`, `stress.sh`). With `EXPECT_LOCKS=1` a reader that sees half-old, half-new data is a failure.

## Statistics format

The server answers `stats` with `key value` lines (for example `cache.hits 80`). The keys each module reports are
listed at the top of `include/stats.h`; `include/stats.h` also has the helper functions to add a number.

## Repository layout

```text
include/   protocol.h ipc.h parse.h session.h log.h stats.h (shared headers), sched.h waitfor.h
src/common/   ipc.c protocol.c log.c session.c stats.c
src/client/   main.c parse.c
src/          sched.c waitfor.c bench.c
tests/     unit tests, stand-in servers (echo, canned, fake_fs_server), IPC fault tests
scripts/   test, demo, benchmark and report scripts
docs/      report sections (docs/report), results (docs/results), bug log, daily notes
```

## Known limitations

- A client keeps one request in flight; the pipe could fill if very many requests waited at once (300
  simultaneous clients were fine).
- Deleting a non-empty directory returns "invalid argument" (the protocol has no "not empty" status).
- A client killed with SIGKILL leaves its reply FIFO in `/tmp`; run `scripts/cleanup_fifos.sh`.
