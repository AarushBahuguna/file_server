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
