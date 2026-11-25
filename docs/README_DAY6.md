# Day 6 — IPC Reliability Tests

This add-on introduces fault-injection tests for Shubham's existing IPC layer.
It does not change production IPC code or any other team member's modules.

## Files

- `tests/day6_fault_server.c` — one-request test server; can send a wrong request ID or delay its response.
- `tests/day6_ipc_client.c` — probe client that checks expected success/failure.
- `scripts/test_ipc_day6.sh` — compiles the probes and runs unavailable-server, wrong-ID, and delayed-response cases.

## Run

From the repository root:

```bash
./scripts/test_ipc_day6.sh
```

The script builds temporary binaries under `/tmp` and removes its temporary FIFO/build directory on exit.

## Notes

The tests deliberately expect `ipc_call()` to fail when no server is available, when only a mismatched request ID is returned, or when the response arrives after the IPC timeout. If the delayed-response case behaves differently because the project's configured timeout is longer than 10 seconds, increase the `sleep(10)` delay in `tests/day6_fault_server.c` and rerun.

The existing echo test remains the sequential/multi-client regression test:

```bash
make echo-test
```

Run the full existing checks after the new tests:

```bash
make check
```
