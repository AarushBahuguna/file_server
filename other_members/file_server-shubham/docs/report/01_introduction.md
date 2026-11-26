# Introduction

## Problem statement

A file system that serves several users at once has to solve problems that a single-user file system
never meets. Two users may write the same file at the same moment, two users may each hold a file the other
one needs, and many small requests compete for one disk head. This project builds a complete miniature
system that faces all of these problems: a simulated disk with a hierarchical file system, a server that
handles many clients in parallel, locking that keeps files consistent, detection of deadlocks, and a choice
of disk-scheduling policies.

## Objectives

1. Store files and directories on a simulated disk and report fragmentation and free space.
2. Let many users use the file system at the same time through a command-line client.
3. Keep file contents correct under concurrent access with per-file readers-writers locks, without starving
   writers.
4. Detect and recover from deadlocks between users.
5. Reduce disk-head movement with FCFS, SCAN and C-SCAN scheduling and measure the difference, with and
   without a buffer cache.

## Team and modules

| Member | Module |
|---|---|
| Arjun Bansal | Simulated disk, allocation, directory tree, file operations, fragmentation report |
| Shubham Gupta | Client, IPC protocol, sessions, logging, statistics output, test scripts, build and Git |
| Shwetank Tiwari | Server, worker thread pool, readers-writers locks, starvation-aware policy, LRU cache with write-back |
| Aarush Bahuguna | Wait-for graph, deadlock detection and recovery, disk request queue, FCFS / SCAN / C-SCAN schedulers |

Each module was written against an agreed set of header files that were frozen on the first day, so every
member could build and test their part alone with stand-in code for the others.

## Structure of this report

Section 2 describes the overall architecture. Sections 3 to 6 describe the four modules in the order a request
meets them. Section 7 summarises testing and results, and Section 8 concludes.
