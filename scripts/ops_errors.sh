#!/bin/bash
# Error handling: every mistake must give a clear message, never a crash or a hang.
source "$(dirname "$0")/lib.sh"
start_server

echo "== file-system errors reported by the server"
run_client dave <<CMDS
create f.txt
create f.txt
mkdir x/y
cd f.txt
rm missing.txt
open missing.txt
open f.txt r
write 3 0 nope
close 3
open f.txt w
write 3 0 abc
read 3 100 10
close 3
exit
CMDS
assert_has "error: create f.txt: already exists"                      "duplicate create"
assert_has "error: mkdir x/y: no such file or directory"              "mkdir without parent"
assert_has "error: cd f.txt: no such file or directory"               "cd into a file"
assert_has "error: delete missing.txt: no such file or directory"     "delete missing file"
assert_has "error: open missing.txt: no such file or directory"       "open missing file"
assert_has "error: write fd 3: permission denied"                     "write to a read-only fd"
assert_has "(empty)"                                                  "read past end of file"

echo "== mistakes caught by the client itself"
run_client dave <<CMDS
read 9 0 5
close 9
write 3 0 hi
bogus
mkdir
mkdir a b
read 3 x 5
open f.txt q
stats nonsense
exit
CMDS
assert_has "error: fd 9 is not an open fd"             "read on unknown fd"
assert_has "error: fd 3 is not an open fd"             "write on unopened fd"
assert_has "unknown command 'bogus'"                   "unknown command"
assert_has "error: usage: mkdir <path>"                "missing argument"
assert_has "too many arguments"                        "extra argument"
assert_has "usage: read <fd> <offset> <length>"        "bad number"
assert_has "error: mode must be r or w"                "bad open mode"
assert_has "usage: stats [table|raw|csv]"              "bad stats format"

echo "== files are released when a client exits without closing them"
run_client dave <<CMDS
open f.txt w
exit
CMDS
run_client dave <<CMDS
open f.txt w
exit
CMDS
assert_has "fd = 3" "second session starts again at fd 3 (old session was cleaned up)"

echo "== server not running"
stop_server
run_client dave <<CMDS
ls
CMDS
assert_has "cannot reach the server" "clear message when the server is down"

summary
