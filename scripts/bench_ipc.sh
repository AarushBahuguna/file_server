#!/bin/bash
# IPC throughput versus number of concurrent clients (echo server = pure IPC cost, no file system).
# Writes docs/results/ipc_bench.csv.   N=1000 ./scripts/bench_ipc.sh
cd "$(dirname "$0")/.." || exit 1
N=${N:-1000}
CSV=docs/results/ipc_bench.csv
mkdir -p docs/results
make -s bin/echo_server bin/echo_client || exit 1
rm -f /tmp/fs_server_fifo
./bin/echo_server > /tmp/bench_server.out 2>&1 &
SRV=$!
stop_bench_server() {                      # echo_server stops on a shutdown request (SIGTERM cannot interrupt its read)
    timeout 5 ./bin/echo_client --shutdown > /dev/null 2>&1
    sleep 0.2
    kill -KILL $SRV 2>/dev/null
    wait $SRV 2>/dev/null
    rm -f /tmp/fs_server_fifo
}
trap stop_bench_server EXIT
for _ in $(seq 1 30); do [ -p /tmp/fs_server_fifo ] && break; sleep 0.1; done

echo "clients,requests_per_client,total_requests,seconds,req_per_sec,avg_latency_ms" > "$CSV"
for c in 1 2 4 8 16 32; do
    T0=$(date +%s.%N)
    PIDS=()
    for i in $(seq 1 "$c"); do ./bin/echo_client "b$i" "$N" > /dev/null & PIDS+=($!); done
    for p in "${PIDS[@]}"; do wait "$p"; done
    T1=$(date +%s.%N)
    awk -v c="$c" -v n="$N" -v t0="$T0" -v t1="$T1" \
        'BEGIN{s=t1-t0; tot=c*n; printf "%d,%d,%d,%.3f,%.0f,%.3f\n", c, n, tot, s, tot/s, s*1000/n}' >> "$CSV"
done
awk -F, '{printf "%-8s %-10s %-10s %-9s %-11s %s\n", $1, $2, $3, $4, $5, $6}' "$CSV"
echo "saved: $CSV"
