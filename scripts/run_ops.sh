#!/bin/bash
# Runs all ops scripts and reports the total. Use SERVER_BIN=./bin/server to test the real server.
cd "$(dirname "$0")/.." || exit 1
total_fail=0
for s in ops_basic ops_dirs ops_errors ops_stats; do
    echo
    echo "################ $s"
    "./scripts/$s.sh" || total_fail=$((total_fail + 1))
done
echo
if [ "$total_fail" -eq 0 ]; then echo "ALL OPS SCRIPTS PASSED"; else echo "$total_fail OPS SCRIPT(S) FAILED"; exit 1; fi
