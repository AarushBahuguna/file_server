#!/bin/bash
# Shared helpers for the ops_*.sh scripts (OWNER: Shubham).
# By default they run against the fake in-memory server. Against the real one:
#     SERVER_BIN=./bin/server scripts/run_ops.sh
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER_BIN=${SERVER_BIN:-$ROOT/bin/fake_fs_server}
CLIENT_BIN=${CLIENT_BIN:-$ROOT/bin/client}
FIFO=/tmp/fs_server_fifo
PASSED=0
FAILED=0
SRV_PID=""
OUT=""

start_server() {
    rm -f "$FIFO"
    "$SERVER_BIN" > /tmp/ops_server.out 2>&1 &
    SRV_PID=$!
    for _ in $(seq 1 30); do [ -p "$FIFO" ] && return 0; sleep 0.1; done
    echo "server did not start (see /tmp/ops_server.out)"; exit 1
}

stop_server() {
    [ -n "$SRV_PID" ] || return 0
    kill -TERM "$SRV_PID" 2>/dev/null
    for _ in 1 2 3 4 5 6 7 8 9 10; do kill -0 "$SRV_PID" 2>/dev/null || break; sleep 0.1; done
    kill -KILL "$SRV_PID" 2>/dev/null
    wait "$SRV_PID" 2>/dev/null
    rm -f "$FIFO"
    SRV_PID=""
}
trap stop_server EXIT

# run_client <user>   (commands come from stdin; stdout+stderr end up in $OUT)
run_client() { OUT=$("$CLIENT_BIN" "$1" 2>&1); }

ok()  { PASSED=$((PASSED + 1)); echo "  ok   : $1"; }
bad() { FAILED=$((FAILED + 1)); echo "  FAIL : $1"; echo "  ---- client output ----"; echo "$OUT" | sed 's/^/  | /'; echo "  -----------------------"; }

assert_has()   { if echo "$OUT" | grep -qF  -- "$1"; then ok "$2"; else bad "$2   (expected to see: $1)"; fi; }
assert_line()  { if echo "$OUT" | grep -qxF -- "$1"; then ok "$2"; else bad "$2   (expected line: $1)"; fi; }
assert_lacks() { if echo "$OUT" | grep -qF  -- "$1"; then bad "$2   (should not see: $1)"; else ok "$2"; fi; }

summary() {
    echo "$(basename "$0"): $PASSED passed, $FAILED failed"
    [ "$FAILED" -eq 0 ]
}
