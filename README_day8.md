# Aarush's Deadlock & Disk-Scheduling Module

## Day 8 Deliverables (Scenarios and Benchmarks)

Day 8 provides race/deadlock demonstrations and a fixed-seed benchmark comparison.

| File | Purpose |
|------|---------|
| `src/bench.c` | FCFS, SCAN, and C-SCAN benchmark for cache on/off modes. |
| `scripts/run_bench.sh` | Compiles and runs five benchmark passes. |
| `scripts/race_demo.sh` | Explains no-lock versus locked writer behavior. |
| `scripts/deadlock_demo.sh` | Runs the two-cycle deadlock test demo. |
| `docs/results/bench_results.csv` | Reproducible benchmark output. |

## Benchmark Workload

- 1000 random blocks
- fixed seeds
- initial head 53
- disk blocks 0–199
- FCFS, SCAN, C-SCAN
- cache on and cache off
- five runs per configuration

## How to Run

```bash
chmod +x scripts/run_bench.sh scripts/race_demo.sh scripts/deadlock_demo.sh
./scripts/run_bench.sh
./scripts/race_demo.sh
./scripts/deadlock_demo.sh
```

## Detector Modes

On-request detection starts DFS from the newly waiting session and detects the cycle immediately after an edge is added. Periodic detection scans all nodes approximately once per second and has at most the polling interval of added delay.

## Definition of Done

- [x] Race demo distinguishes locked and unlocked writes
- [x] Deadlock demo illustrates opposite lock order
- [x] Benchmark supports every policy/cache configuration
- [x] Five-run fixed-seed script is available
- [x] CSV is written reproducibly
