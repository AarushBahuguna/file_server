#!/usr/bin/env bash
# Day 6: IPC reliability and fault-injection tests.
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="${TMPDIR:-/tmp}/file_server_day6_build_$$"
mkdir -p "$BUILD_DIR"
SERVER_FIFO=/tmp/fs_server_fifo
cleanup() {
    [ -n "${SERVER_PID:-}" ] && kill "$SERVER_PID" 2>/dev/null || true
    rm -f "$SERVER_FIFO"
    rm -rf "$BUILD_DIR"
}
trap cleanup EXIT INT TERM

COMMON=(src/common/ipc.c src/common/protocol.c src/common/log.c src/common/session.c)
CFLAGS=(-Wall -Wextra -g -pthread -Iinclude)
gcc "${CFLAGS[@]}" tests/day6_fault_server.c "${COMMON[@]}" -o "$BUILD_DIR/fault_server" -pthread
gcc "${CFLAGS[@]}" tests/day6_ipc_client.c "${COMMON[@]}" -o "$BUILD_DIR/probe_client" -pthread

start_server() {
    local mode="$1"
    rm -f "$SERVER_FIFO"
    "$BUILD_DIR/fault_server" "$mode" >"$BUILD_DIR/server.log" 2>&1 &
    SERVER_PID=$!
    for _ in $(seq 1 50); do
        [ -p "$SERVER_FIFO" ] && return 0
        kill -0 "$SERVER_PID" 2>/dev/null || { cat "$BUILD_DIR/server.log"; return 1; }
        sleep 0.1
    done
    echo "FAIL: server FIFO did not appear" >&2
    return 1
}

stop_server() {
    if [ -n "${SERVER_PID:-}" ]; then
        wait "$SERVER_PID" 2>/dev/null || true
        unset SERVER_PID
    fi
    rm -f "$SERVER_FIFO"
}

echo "[1/3] Server unavailable"
rm -f "$SERVER_FIFO"
"$BUILD_DIR/probe_client" failure

echo "[2/3] Wrong request ID"
start_server wrong-id
"$BUILD_DIR/probe_client" failure
stop_server

echo "[3/3] Delayed response"
start_server delay
"$BUILD_DIR/probe_client" failure
stop_server

echo "ALL DAY 6 IPC FAULT TESTS PASSED"
