# Aarush's Deadlock & Disk-Scheduling Module

## Day 10 Deliverables (Demos and Slides)

Day 10 is the final presentation and rehearsal checkpoint.

| File | Purpose |
|------|---------|
| `scripts/deadlock_demo.sh` | Three-run deadlock-cycle and victim-recovery demo. |
| `scripts/performance_demo.sh` | Three-run textbook and benchmark performance demo. |
| `scripts/rehearse_demos.sh` | Automated compilation and full rehearsal runner. |
| `docs/slides.md` | Presentation slide outline and final talking points. |
| `docs/demo_qa.md` | Answers to likely technical presentation questions. |

## How to Rehearse

```bash
cd /home/aarushb/Documents/file_server/file_server
./scripts/deadlock_demo.sh
./scripts/performance_demo.sh
./scripts/rehearse_demos.sh
```

## Deadlock Demo Sequence

1. Session 0 waits for Session 1.
2. Session 1 waits for Session 0.
3. DFS detects cycle `0 -> 1 -> 0`.
4. The youngest session is selected as victim.
5. The victim is aborted and its edges are removed.
6. The remaining session completes.

## Performance Demo Sequence

1. Show textbook totals for FCFS, SCAN, and C-SCAN.
2. Run random scheduler workloads.
3. Run cache-on/cache-off benchmark configurations.
4. Show CSV and report values.

## Project Definition of Done

- [x] FCFS, SCAN, and C-SCAN textbook totals pass
- [x] Every request is served exactly once under load
- [x] Two-cycle and three-cycle deadlocks are covered
- [x] Wait chains do not produce false positives
- [x] Deadlock and integration unit suites pass 100 repetitions
- [x] Race and locking behavior is documented
- [x] Benchmark covers all six policy/cache combinations
- [x] Benchmark CSV and report section exist
- [x] Deadlock demo passes three rehearsals
- [x] Performance demo passes three rehearsals
- [x] Slides and likely answers are prepared

## Verification Result

The final rehearsal completed successfully:

```text
Rehearing deadlock demo run 1
Rehearing performance demo run 1
Rehearing deadlock demo run 2
Rehearing performance demo run 2
Rehearing deadlock demo run 3
Rehearing performance demo run 3
Day 10 rehearsal complete: both demos passed 3 runs; unit suites passed 100 runs
```
