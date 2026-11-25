#!/bin/bash
# Day 3: run the CLI against the dummy server and check the output.
cd "$(dirname "$0")/.." || exit 1
make -s bin/client bin/dummy_server bin/echo_client || exit 1

rm -f /tmp/fs_server_fifo
./bin/dummy_server > /tmp/dummy_server.log 2>&1 &
SRV=$!
for _ in $(seq 1 20); do [ -p /tmp/fs_server_fifo ] && break; sleep 0.1; done

BIG=$(head -c 600 /dev/zero | tr '\0' 'x')
OUT=$(./bin/client tester 2>&1 <<CMDS
mkdir docs
cd docs
ls
create a.txt
open a.txt w
write 3 0 hello world
write 3 0 $BIG
read 3 0 5
read 3 0 5000
stats
rm missing.txt
cd missing
bogus
mkdir
close 3
exit
CMDS
)
./bin/echo_client --shutdown
wait "$SRV" 2>/dev/null

echo "$OUT"
echo "-----"
fail=0
check() { echo "$OUT" | grep -qF -- "$1" && echo "ok   : $1" || { echo "FAIL : $1"; fail=1; }; }
check "fd = 3"
check "a.txt"
check "wrote 11 bytes"
check "wrote 600 bytes"
check "dummy"
check "dummy data from the dummy server"
check "dummy stats"
check "error: no such file or directory"
check "unknown command 'bogus'"
check "usage: mkdir <path>"
[ $fail -eq 0 ] && echo "CLI TEST PASSED" || { echo "CLI TEST FAILED"; exit 1; }
