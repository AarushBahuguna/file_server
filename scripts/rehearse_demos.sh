#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

compile_and_run() {
    gcc -Wall -Wextra -I include src/sched.c tests/mock_disk.c tests/test_sched.c -lpthread -o test_sched
    gcc -Wall -Wextra -I include src/waitfor.c tests/test_deadlock.c -lpthread -o test_deadlock
    gcc -Wall -Wextra -I include src/sched.c tests/mock_disk.c tests/test_integration.c -lpthread -o test_integration
    gcc -Wall -Wextra -I include src/bench.c -o bench

    ./test_sched >/dev/null
    ./test_deadlock >/dev/null
    ./test_integration >/dev/null
}

compile_and_run

for run in 1 2 3; do
    echo "Rehearing deadlock demo run $run"
    ./scripts/deadlock_demo.sh >/dev/null

    echo "Rehearing performance demo run $run"
    ./scripts/performance_demo.sh >/dev/null

done

./scripts/run_bench.sh >/dev/null

for i in $(seq 1 100); do
    ./test_sched >/dev/null
    ./test_deadlock >/dev/null
done

echo 'Day 10 rehearsal complete: both demos passed 3 runs; unit suites passed 100 runs'
