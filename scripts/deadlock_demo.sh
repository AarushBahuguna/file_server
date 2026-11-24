#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

gcc -Wall -Wextra -I include src/waitfor.c tests/test_deadlock.c -lpthread -o test_deadlock

printf '%s\n' '=== Deadlock scenario: opposite lock order ==='
printf '%s\n' 'Session 0 waits for Session 1'
printf '%s\n' 'Session 1 waits for Session 0'
printf '%s\n' 'Detecting cycle and selecting the youngest victim...'
./test_deadlock
