#!/bin/bash
# Many clients, many operations: every client creates, writes, reads back, verifies and deletes its own files.
#   CLIENTS=16 ITERS=20 ./scripts/stress.sh
source "$(dirname "$0")/lib.sh"
start_server
CLIENTS=${CLIENTS:-16}
ITERS=${ITERS:-20}
TMP=$(mktemp -d)
trap 'stop_server; rm -rf "$TMP"' EXIT

run_client setup <<CMDS
mkdir /stress
exit
CMDS

client() {
    local j=$1
    {
        for i in $(seq 1 "$ITERS"); do
            f=/stress/c${j}_$i
            echo "create $f"; echo "open $f w"; echo "write 3 0 data-$j-$i"; echo "close 3"
            echo "open $f r"; echo "read 3 0 64"; echo "close 3"; echo "rm $f"
        done
        echo exit
    } | "$CLIENT_BIN" "s$j" > "$TMP/c$j.log" 2>&1
}

echo "== $CLIENTS clients x $ITERS iterations (create, write, read back, delete)"
T0=$(date +%s.%N)
PIDS=()
for j in $(seq 1 "$CLIENTS"); do client "$j" & PIDS+=($!); done
for p in "${PIDS[@]}"; do wait "$p"; done
T1=$(date +%s.%N)

fail=0
for j in $(seq 1 "$CLIENTS"); do
    L="$TMP/c$j.log"
    w=$(grep -c '^wrote ' "$L")
    ok=$(grep -cE "^data-$j-[0-9]+$" "$L")
    e=$(grep -cE '^error:' "$L")
    if [ "$w" -ne "$ITERS" ] || [ "$ok" -ne "$ITERS" ] || [ "$e" -ne 0 ]; then
        echo "  FAIL client $j: writes=$w verified_reads=$ok errors=$e (expected $ITERS/$ITERS/0)"; fail=1
    fi
done
TOTAL=$((CLIENTS * ITERS * 8))
awk -v t0="$T0" -v t1="$T1" -v n="$TOTAL" 'BEGIN{s=t1-t0; printf "  %d requests in %.2f s = %.0f requests/s\n", n, s, n/s}'

run_client check <<CMDS
ls /stress
exit
CMDS
echo "$OUT" | grep -q 'c[0-9]*_' && { echo "  FAIL: leftover files in /stress"; fail=1; }
[ $fail -eq 0 ] && echo "STRESS PASSED ($CLIENTS x $ITERS verified)" || { echo "STRESS FAILED"; exit 1; }
