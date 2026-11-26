# Arjun Storage Layer — Integrated README

## Overview

Arjun's storage implementation is integrated into the root project. It provides the
simulated 1 MiB disk, bitmap and inode allocation, indexed file allocation,
directories, path resolution, file I/O, persistence, reporting, and consistency checks.

## Integrated files

| File | Purpose |
|------|---------|
| `include/common.h` | Disk, block, inode, path, and file-size constants. |
| `include/disk.h` | Raw disk and persistence API. |
| `include/fs.h` | Public filesystem API. |
| `src/disk.c` | 2048-block in-memory disk with load/save support. |
| `src/alloc.c` | Block and inode allocation. |
| `src/dir.c` | Directory entries and path resolution. |
| `src/fs.c` | File, directory, open-table, report, and consistency operations. |
| `src/fs_internal.h` | Internal allocation and mapping declarations. |
| `tests/test_fs.c` | Storage unit and persistence test. |

## On-disk layout

| Blocks | Purpose |
|------|---------|
| 0 | Superblock |
| 1 | Free-block bitmap |
| 2–17 | 128 inode records |
| 18–2047 | Data blocks |

Block size is 512 bytes. Inodes contain 10 direct pointers and one indirect pointer,
allowing 138 blocks or 70,656 bytes per file.

## API adaptations

Arjun's existing `disk_read` and `disk_write` compatibility wrappers remain available.
In the full integration path, Shwetank's cache will sit before these raw functions:

```text
fs_* -> cache_get/cache_put -> sched_submit -> disk_raw_read/disk_raw_write
```

The scheduler remains the only intended caller of raw disk operations after cache
integration. Storage metadata remains protected by Arjun's metadata mutex.

## Build and test

```bash
cd /home/aarushb/Documents/file_server/file_server
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -Isrc \
    src/disk.c src/alloc.c src/dir.c src/fs.c tests/test_fs.c \
    -lpthread -o test_fs
./test_fs
```

## Verification

The integrated storage test passed with nested directories, direct/indirect file I/O,
delete and consistency checks, bad-path behavior, disk persistence, and reporting.

## Integration notes

- Cache flush must occur before `disk_save`.
- Metadata operations use Arjun's metadata mutex.
- File-content locks belong to Shwetank's lock manager.
- Crash journaling and permissions are not implemented in this storage version.
- Shubham's client protocol can call these APIs through Shwetank's dispatcher in the
  future integration phase.
