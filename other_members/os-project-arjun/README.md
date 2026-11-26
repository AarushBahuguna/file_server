# Multi-User File System — Storage Layer

C11 educational filesystem implementing the storage lead plan. The disk is simulated in memory and persisted to `disk.img` with `disk_load`/`disk_save`.

## Build and test

```sh
make
make test
make asan
make valgrind   # requires Valgrind installed
```

The build uses pthread mutexes and does not require a server. `disk.img` is generated, not committed.

## Layout

| Blocks | Purpose |
|---|---|
| 0 | superblock |
| 1 | free-block bitmap |
| 2–17 | 128 × 64-byte inodes |
| 18–2047 | data blocks |

Block size is 512 bytes. Directories use 32-byte entries (28-byte name + inode number). Each inode has 10 direct pointers and one single-indirect pointer with 128 entries; maximum file size is 70,656 bytes.

## API notes

All path operations accept a cwd inode. `fs_cd` returns the new cwd inode. `fs_open` returns a process-local descriptor. `fs_read`/`fs_write` take explicit offsets and return byte counts or negative errno values. Mode is currently stored for interface compatibility; access-mode enforcement can be added by the dispatcher policy.

## Integration contract

The current `disk_read`/`disk_write` functions are compatibility wrappers around `disk_raw_read`/`disk_raw_write`. On integration day, replace those wrappers with Shwetank's cache calls; the scheduler should remain the sole caller of raw disk operations if that is the team's agreed design. Ensure cache flush occurs before `disk_save`.

The storage core serializes filesystem metadata operations with one metadata mutex. This is intentionally conservative for correctness; it is not a fine-grained per-file concurrency implementation. Coordinate lock order with the cache/scheduler owners before merging. The dispatcher should maintain one cwd inode per client session.

## Limitations to discuss during integration

- This is an educational single-process filesystem, not a production POSIX filesystem.
- The open-file table is global to the process; there is no per-client credential model.
- `permissions` and `mode` are stored but not yet enforced.
- Directory entries are fixed-size; directory growth is supported through direct blocks only (up to 160 slots).
- Crash consistency/journaling is not implemented. `disk_save` is an orderly shutdown snapshot.
- Disk-full writes may return a successful short write if some bytes were written before allocation failed; callers must check the returned count.
