#!/bin/bash
# The stats command: table, raw and csv formats.
source "$(dirname "$0")/lib.sh"
start_server

run_client erin <<CMDS
create a.txt
open a.txt w
write 3 0 $(head -c 1200 /dev/zero | tr '\0' 'q')
close 3
create b.txt
stats
exit
CMDS
echo "== stats (table)"
assert_line "Disk"                 "Disk section"
assert_has  "Total blocks"         "total blocks row"
assert_has  "Usage (%)"            "derived usage row"
assert_line "Server"               "Server section"
assert_has  "Active sessions"      "session count row"
echo "$OUT" | sed -n '/^Disk$/,$p' | sed 's/^/  | /'

run_client erin <<CMDS
stats csv
exit
CMDS
echo "== stats csv"
assert_line "key,value"            "csv header"
assert_has  "disk.files,2"         "two files created"
assert_has  "disk.used_blocks,3"   "1200 bytes = 3 blocks of 512"

run_client erin <<CMDS
stats raw
exit
CMDS
echo "== stats raw"
assert_has  "disk.total_blocks 1024" "raw key value lines"

summary
