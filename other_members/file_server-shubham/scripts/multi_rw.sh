#!/bin/bash
# One writer keeps rewriting a 3000-byte file (all 'A' <-> all 'B') while several readers read it.
# With correct readers-writers locking a reader must always see ALL 'A' or ALL 'B', never a mix.
#   EXPECT_LOCKS=1  -> a mixed ("torn") read is a failure (use this against the REAL server)
#   EXPECT_LOCKS=0  -> mixed reads are only reported (the fake server has no file locks)
source "$(dirname "$0")/lib.sh"
start_server
SIZE=3000
READERS=${READERS:-5}
ROUNDS=${ROUNDS:-15}
EXPECT_LOCKS=${EXPECT_LOCKS:-0}
A=$(head -c $SIZE /dev/zero | tr '\0' 'A')
B=$(head -c $SIZE /dev/zero | tr '\0' 'B')
TMP=$(mktemp -d)
trap 'stop_server; rm -rf "$TMP"' EXIT

run_client setup <<CMDS
create /shared.txt
open /shared.txt w
write 3 0 $A
close 3
exit
CMDS

writer() {
    {
        for _ in $(seq 1 "$ROUNDS"); do
            for X in "$B" "$A"; do
                echo "open /shared.txt w"; echo "write 3 0 $X"; echo "close 3"
            done
        done
        echo exit
    } | "$CLIENT_BIN" writer > "$TMP/writer.log" 2>&1
}
reader() {
    {
        for _ in $(seq 1 $((ROUNDS * 2))); do
            echo "open /shared.txt r"; echo "read 3 0 $SIZE"; echo "close 3"
        done
        echo exit
    } | "$CLIENT_BIN" "reader$1" > "$TMP/reader$1.log" 2>&1
}

echo "== 1 writer + $READERS readers, $((ROUNDS * 2)) rounds each"
writer & WP=$!
RP=()
for r in $(seq 1 "$READERS"); do reader "$r" & RP+=($!); done
wait "$WP"
for p in "${RP[@]}"; do wait "$p"; done

EXPECTED=$((READERS * ROUNDS * 2))
READS=$(cat "$TMP"/reader*.log | grep -cE "^[AB]{$SIZE}$")
TORN=$(cat "$TMP"/reader*.log | grep -E '^[AB]+$' | grep A | grep -cE 'B')
ERRS=$(cat "$TMP"/*.log | grep -cE '^error:')
echo "  full reads : $READS (expected $EXPECTED)"
echo "  torn reads : $TORN (a reader saw a mix of old and new data)"
echo "  errors     : $ERRS"

fail=0
[ "$ERRS" -eq 0 ] || { echo "FAIL: clients reported errors"; head -3 <(cat "$TMP"/*.log | grep -E '^error:'); fail=1; }
[ "$READS" -eq "$EXPECTED" ] || { echo "FAIL: some reads did not return the whole file"; fail=1; }
if [ "$TORN" -gt 0 ]; then
    if [ "$EXPECT_LOCKS" = "1" ]; then echo "FAIL: readers saw torn data, so readers-writers locking is not working"; fail=1
    else echo "NOTE: torn reads are expected without file locks (fake server). The real server must give 0."; fi
fi
[ $fail -eq 0 ] && echo "MULTI RW PASSED" || { echo "MULTI RW FAILED"; exit 1; }
