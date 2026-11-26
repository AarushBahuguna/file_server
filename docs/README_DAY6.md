# Day 6 - IPC reliability tests

Fault-injection tests for the IPC layer (`ipc_call` with timeout and request-id matching).
They do not change any production code.

## Files
- `tests/day6_fault_server.c` - test server with modes `normal`, `wrong-id`, `delay`, `stale`.
- `tests/day6_ipc_client.c`   - probe client; checks the expected result and (for failures) the exact errno.
- `scripts/test_ipc_day6.sh`  - builds both into a temp dir and runs the four cases.

## Cases
| # | Situation | Expected |
|---|-----------|----------|
| 1 | server not running | `ipc_call` fails with `ENOENT` |
| 2 | server answers with a wrong request id | reply is ignored, call fails with `ETIMEDOUT` |
| 3 | server answers after the 1 s timeout (it sleeps 3 s) | call fails with `ETIMEDOUT` |
| 4 | call 1 times out and its reply arrives late; call 2 follows | the late reply is skipped and call 2 gets its own reply |

Case 4 is the important one: it proves a late reply is never mistaken for the answer to the next request.
Every probe has a 15 s hard limit, so a hang counts as a failure.

## Run
    make ipc-test        # or ./scripts/test_ipc_day6.sh
    make check           # runs everything, including this
