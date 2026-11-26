#!/bin/bash
# Narrated demo for the presentation (about 2 minutes).
#   ./scripts/demo.sh                      runs straight through, fake server
#   DEMO_PAUSE=1 ./scripts/demo.sh         waits for Enter before every step
#   SERVER_BIN=./bin/server ./scripts/demo.sh   same demo against the real server
source "$(dirname "$0")/lib.sh"
export FS_CLIENT_LOG=/tmp/demo_client.log
rm -f "$FS_CLIENT_LOG"
start_server
export SERVER_RUNNING=1      # sub-scripts use this server instead of starting their own

step() {
    echo; echo "=================================================================="
    echo "  $1"
    echo "=================================================================="
    if [ -n "${DEMO_PAUSE:-}" ] && [ -r /dev/tty ]; then read -r -p "  [Enter to run] " _ < /dev/tty; fi
}
demo() {                       # demo <user>   (commands on stdin)
    local user=$1 cmds
    cmds=$(cat)
    echo "$cmds" | sed "s/^/  $user> /"
    echo "  ---- output ----"
    printf '%s\nexit\n' "$cmds" | "$CLIENT_BIN" "$user" 2>&1 | sed 's/^/  /'
}

step "1. alice creates a directory and a file, and writes to it"
demo alice <<CMDS
mkdir /projects
cd /projects
create report.txt
open report.txt w
write 3 0 Quarterly numbers: 42, 17, 99
close 3
ls
CMDS

step "2. bob (another user, another session) reads alice's file"
demo bob <<CMDS
cd /projects
open report.txt
read 3 0 100
close 3
CMDS

step "3. every session has its own current directory"
demo carol <<CMDS
cd /projects
ls
CMDS
demo dave <<CMDS
ls
CMDS

step "4. mistakes get clear messages (not crashes, not hangs)"
demo erin <<CMDS
open nothing.txt
create /projects/report.txt
read 9 0 5
mkdir
frobnicate
CMDS

step "5. eight clients working at the same time, all results verified"
./scripts/multi_client.sh 8 demo | tail -1

step "6. readers and a writer on one file (consistency check)"
EXPECT_LOCKS="${EXPECT_LOCKS:-0}" READERS=4 ROUNDS=8 ./scripts/multi_rw.sh | tail -4

step "7. statistics: table for people, csv for graphs"
demo alice <<CMDS
stats
CMDS
demo alice <<CMDS
stats csv
CMDS

step "8. what the clients did, from their log (counts, errors, latency)"
./scripts/log_report.sh "$FS_CLIENT_LOG"

echo; echo "Demo finished."
