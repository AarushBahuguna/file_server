# Aarush's Deadlock & Disk-Scheduling Module

## Day 9 Deliverables (Results and Report)

Day 9 turns benchmark output and deadlock behavior into a report section.

| File | Purpose |
|------|---------|
| `docs/results/bench_results.csv` | Benchmarks by policy, cache state, and seed. |
| `docs/results/detector_modes.txt` | On-request and periodic detection timing notes. |
| `docs/deadlock_scheduling_section.md` | Final deadlock and disk-scheduling report section. |

## Report Questions

The report compares FCFS, SCAN, and C-SCAN, explains cache hit effects, records
C-SCAN wrap movement and SCAN boundary stops, and describes recovery after a
wait-for graph cycle.

## How to Compile and Verify

```bash
./scripts/run_bench.sh
cat docs/results/bench_results.csv
cat docs/results/detector_modes.txt
```

## Definition of Done

- [x] Benchmark CSV exists
- [x] Detector timing notes exist
- [x] Report section explains graph cycles and victim recovery
- [x] Policy, cache, and batch-window behavior are analyzed
