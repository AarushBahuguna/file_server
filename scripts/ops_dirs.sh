#!/bin/bash
# Directory operations: nested mkdir, cd with relative paths and '..', ls, deleting directories.
source "$(dirname "$0")/lib.sh"
start_server

echo "== build a tree and walk around it"
run_client carol <<CMDS
mkdir a
mkdir a/b
cd a/b
create deep.txt
cd ../..
cd a
ls
cd b
ls
cd /
ls
rm a
exit
CMDS
assert_line "/a/b"   "cd a/b prints the new directory"
assert_line "/"      "cd ../.. goes back to the root"
assert_line "/a"     "relative cd works"
assert_line "b/"     "ls shows sub-directory with a trailing slash"
assert_line "deep.txt" "ls inside a/b shows the file"
assert_line "a/"     "ls of / shows a/"
assert_has  "error: delete a: invalid argument" "deleting a non-empty directory is refused"

echo "== clean the tree bottom-up"
run_client carol <<CMDS
rm a/b/deep.txt
rm a/b
rm a
exit
CMDS
assert_lacks "error" "files and directories deleted without errors"

run_client carol <<CMDS
ls
exit
CMDS
assert_lacks "a/" "root is empty again"

summary
