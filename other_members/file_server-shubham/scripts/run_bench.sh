#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

mkdir -p docs/results
gcc -Wall -Wextra -I include src/bench.c -o bench

for run in $(seq 1 5); do
    ./bench > "docs/results/bench_run_${run}.txt"
done

./bench

echo "benchmarks completed: 5 runs for every policy/cache configuration"
echo "CSV: docs/results/bench_results.csv"
