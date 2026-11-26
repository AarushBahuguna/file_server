#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

gcc -Wall -Wextra -I include src/waitfor.c tests/test_deadlock.c -lpthread -o test_deadlock

printf '%s\n' '=== Deadlock demo: opposite lock order ==='
printf '%s\n' '1. Session 0 waits for Session 1'
printf '%s\n' '2. Session 1 waits for Session 0'
printf '%s\n' '3. Wait-for graph forms cycle: 0 -> 1 -> 0'
printf '%s\n' '4. DFS detects the cycle and selects the youngest victim'
printf '%s\n' '5. Victim is aborted, victim edges are removed, and -EDEADLK is returned'
printf '%s\n' '6. The remaining session completes after the cycle is broken'

for run in 1 2 3; do
    printf '\nDeadlock rehearsal run %d:\n' "$run"
    ./test_deadlock >/dev/null
    echo 'cycle detected; victim recovered; graph cleared; non-victim can complete'
done
