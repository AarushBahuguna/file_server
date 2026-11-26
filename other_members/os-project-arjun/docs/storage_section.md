# Storage Layer — Design and Results

## Disk layout

The simulated disk contains 2,048 blocks of 512 bytes (1 MiB). Block 0 stores the superblock, block 1 stores the free-block bitmap, blocks 2–17 store 128 inodes (64 bytes each), and blocks 18–2047 are data blocks. The root directory is inode 0.

## Allocation strategy

Indexed allocation is used. An inode contains ten direct pointers and one single-indirect pointer. The indirect block stores 128 physical block numbers. Thus the maximum file size is `(10 + 128) × 512 = 70,656 bytes`. Indexed allocation avoids external fragmentation within an individual file and supports direct logical-block lookup. Linked allocation makes random access slower; contiguous allocation can suffer from external fragmentation.

## Directories and paths

A directory contains 32-byte entries: a 28-byte name and a 4-byte inode number. Path resolution supports absolute and cwd-relative paths, `.`, and `..`. The root's parent is itself. Directories grow using direct data blocks.

## Synchronization and persistence

Filesystem operations are serialized by a metadata mutex to protect allocation metadata, inode updates, and directory changes. Disk-array access and counters have their own mutex. For the full team stack, the intended chain is `fs_* → cache → scheduler → disk_raw_*`; flush the cache before saving the disk image.

## Fragmentation measurements

Run `make test` and use the `fs_report` output for a live snapshot. The CSV in `results/fragmentation.csv` is a reproducible measurement template; populate it by running the create/delete patterns described below on the integrated build. The current test suite validates allocation/reclamation and direct-to-indirect I/O but does not claim benchmark measurements from a multi-client server.

Suggested patterns: many small files; a few large files; alternating create/delete; then record total free blocks, largest free run, logical file bytes, and allocated file bytes. Internal fragmentation is allocated file bytes minus logical file bytes. External fragmentation is reported as largest free run relative to total free blocks.
