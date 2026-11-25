# Day 1 contract checklist (freeze before anyone codes)

| Header | Owner | Status |
|---|---|---|
| include/protocol.h (request/response, commands, status codes) | Shubham | DONE |
| include/ipc.h (FIFO helper API) | Shubham | DONE |
| include/sched.h, include/waitfor.h | Aarush | DONE (already in repo) |
| include/fs.h (BLOCK_SIZE, DISK_SIZE, disk_read/write, fs_*) | Arjun | TODO |
| include/lock.h (lock_acquire/lock_release) | Shwetank | TODO |
| include/cache.h (cache_get/cache_put) | Shwetank | TODO |

Limits: write max 512 bytes per request, read max 3072 bytes per response.
Rules: one branch per person, merge to main only after tests pass,
15-minute daily sync, every module gets a test_*.c or shell script.
