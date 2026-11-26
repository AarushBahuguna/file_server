#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

gcc -Wall -Wextra -I include src/sched.c tests/mock_disk.c tests/test_sched.c -lpthread -o test_sched
gcc -Wall -Wextra -I include src/bench.c -o bench

printf '%s\n' '=== Performance demo ==='
printf '%s\n' 'Textbook example: head 53, disk 0..199'
printf '%s\n' 'FCFS = 640 | SCAN = 236 | C-SCAN = 382'

for run in 1 2 3; do
    printf '\nPerformance rehearsal run %d:\n' "$run"
    ./test_sched
    ./bench | grep -E 'cache=' | cat
done
