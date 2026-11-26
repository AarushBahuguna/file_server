# Bug log

## Client and IPC (found and fixed)

| # | Problem | How it was found | Fix | Test |
|---|---|---|---|---|
| 1 | Late reply to a timed-out request was returned for the next request | design review of `ipc_call` | request-id matching in `ipc_call` | IPC fault test, case 4 |
| 2 | Client could wait for ever if the server died | design review | `poll` timeout (30 s, `FS_TIMEOUT_MS`) | IPC fault test, case 3 |
| 3 | Client that exited without closing files kept their locks | design review | client closes its files; session logout returns open files | `ops_errors.sh` |
| 4 | `multi_client.sh` reported success although a command had failed (`pwd` is not a command) | code review of a script written by someone else | script now checks every client's output and exit code | run with server stopped and with a bad command |
| 5 | IPC fault tests passed for any failure, even for the wrong reason | mutation check (broke `ipc_call` on purpose) | tests check the exact `errno` and have a 15 s limit | IPC fault tests, cases 1 to 4 |
| 6 | `demo.sh` lost its server because a sub-script started a second one and removed the FIFO | first run of the demo | `SERVER_RUNNING=1` makes sub-scripts reuse the running server | `demo.sh` |
| 7 | Benchmark script hung at the end: the echo server ignores SIGTERM while blocked in `read` | first run of the benchmark | stop it with a shutdown request | `bench_ipc.sh` |

## Known limitations (not bugs in the code, but worth knowing)

| # | Limitation | Reason |
|---|---|---|
| 1 | Deleting a non-empty directory gives "invalid argument" | the frozen `protocol.h` has no "not empty" status |
| 2 | More than about 90 requests waiting at once could fill the 64 KB pipe | each client has one request in flight; 300 clients at once did not reach it |
| 3 | A client killed with SIGKILL leaves `/tmp/fs_client_<pid>` | the process cannot clean up; run `scripts/cleanup_fifos.sh` |

## Integration bugs (Day 7 onward: fill in while running `./scripts/integration_real.sh`)

| # | Problem | Module / owner | Found by | Status |
|---|---|---|---|---|
| | | | | |
