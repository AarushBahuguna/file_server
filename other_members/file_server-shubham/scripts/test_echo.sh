#!/bin/bash
# Day 2: start echo server, run N clients at the same time, check all pass.
# usage: scripts/test_echo.sh [clients] [requests_per_client]
CLIENTS=${1:-5}
COUNT=${2:-50}
cd "$(dirname "$0")/.." || exit 1
make -s bin/echo_server bin/echo_client || exit 1

rm -f /tmp/fs_server_fifo
./bin/echo_server > /tmp/echo_server.log 2>&1 &
SRV=$!
for _ in $(seq 1 20); do [ -p /tmp/fs_server_fifo ] && break; sleep 0.1; done

pids=()
for i in $(seq 1 "$CLIENTS"); do
    ./bin/echo_client "user$i" "$COUNT" &
    pids+=($!)
done

fail=0
for p in "${pids[@]}"; do wait "$p" || fail=1; done

./bin/echo_client --shutdown
wait "$SRV" 2>/dev/null
cat /tmp/echo_server.log

if [ $fail -eq 0 ]; then echo "ALL $CLIENTS CLIENTS PASSED"; else echo "SOME CLIENT FAILED"; exit 1; fi
