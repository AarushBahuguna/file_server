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
