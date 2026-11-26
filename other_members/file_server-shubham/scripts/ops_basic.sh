#!/bin/bash
# Basic file operations: create, write, read, list, delete; data survives across users/sessions.
# Note: the first open in a session returns fd 3 (SESS_FIRST_FD), so the scripts use fd 3.
source "$(dirname "$0")/lib.sh"
start_server

BIG=$(head -c 4000 /dev/zero | tr '\0' 'z')       # 4000 bytes = 8 write chunks, 2 read chunks

echo "== alice creates and writes a file"
run_client alice <<CMDS
mkdir docs
cd docs
create notes.txt
open notes.txt w
write 3 0 hello from alice
close 3
create big.txt
open big.txt w
write 3 0 $BIG
close 3
exit
CMDS
assert_has  "wrote 16 bytes"   "small write reports 16 bytes"
assert_has  "wrote 4000 bytes" "large write is split into chunks and fully written"

echo "== bob (another user, new session) reads it back"
run_client bob <<CMDS
cd docs
ls
open notes.txt
read 3 0 16
read 3 6 4
read 3 0 100
close 3
open big.txt
read 3 0 4000
exit
CMDS
assert_has   "notes.txt"          "ls shows the file"
assert_line  "hello from alice"   "full read returns what alice wrote"
assert_line  "from"               "read with an offset"
assert_has   "$BIG"               "4000-byte read (two chunks) matches"

echo "== alice deletes it"
run_client alice <<CMDS
rm docs/notes.txt
open docs/notes.txt
exit
CMDS
assert_has "error: open docs/notes.txt: no such file or directory" "deleted file cannot be opened"

echo "== other file is untouched"
run_client bob <<CMDS
ls docs
exit
CMDS
assert_has   "big.txt"    "big.txt still there"
assert_lacks "notes.txt"  "notes.txt is gone"

summary
