#!/bin/bash
# Run the CLI against the dummy server (which uses the real session + log modules).
cd "$(dirname "$0")/.." || exit 1
make -s bin/client bin/dummy_server bin/echo_client || exit 1

rm -f /tmp/fs_server_fifo /tmp/dummy_server.log /tmp/client_test.log
./bin/dummy_server > /tmp/dummy_server.out 2>&1 &
SRV=$!
for _ in $(seq 1 20); do [ -p /tmp/fs_server_fifo ] && break; sleep 0.1; done

BIG=$(head -c 600 /dev/zero | tr '\0' 'x')
OUT=$(FS_CLIENT_LOG=/tmp/client_test.log ./bin/client tester 2>&1 <<CMDS
mkdir docs
cd docs
cd sub
cd ..
ls
create a.txt
open a.txt w
open a.txt
write 3 0 hello world
write 3 0 $BIG
read 3 0 5
read 3 0 5000
read 9 0 5
close 4
stats
rm missing.txt
cd missing
bogus
mkdir
exit
CMDS
)
./bin/echo_client --shutdown
wait "$SRV" 2>/dev/null

echo "$OUT"
echo "-----"
fail=0
check()  { echo "$OUT" | grep -qF  -- "$1" && echo "ok   : $1" || { echo "FAIL : $1"; fail=1; }; }
checkx() { echo "$OUT" | grep -qxF -- "$1" && echo "ok   : $1" || { echo "FAIL : $1"; fail=1; }; }
checkf() { grep -qE -- "$2" "$1" && echo "ok   : $1 has $2" || { echo "FAIL : $1 lacks $2"; fail=1; }; }

checkx "/docs/sub"
checkx "/docs"
checkx "fd = 3"
checkx "fd = 4"
check "wrote 11 bytes"
check "wrote 600 bytes"
check "dummy data from the dummy server"
check "dummy stats"
check "sessions: 1 active"
check "not an open fd"
check "error: delete missing.txt: no such file or directory"
check "error: cd missing: no such file or directory"
check "unknown command 'bogus'"
check "usage: mkdir <path>"
checkf /tmp/dummy_server.log "cmd=MKDIR path=docs .*OK"
checkf /tmp/dummy_server.log "cmd=CLOSE .*fd=3"             # client closed fd 3 itself at exit
checkf /tmp/dummy_server.log "cmd=LOGOUT"
checkf /tmp/client_test.log  "user tester logged in"
checkf /tmp/client_test.log  "cmd=OPEN .*\([0-9.]+ ms\)"
[ $fail -eq 0 ] && echo "CLI TEST PASSED" || { echo "CLI TEST FAILED"; exit 1; }
