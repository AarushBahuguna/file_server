# Server, Synchronization and Cache

TODO(Shwetank): write this section. Cover: the server loop and worker thread pool, the per-file
readers-writers lock and the policy that stops writers from starving, the LRU buffer cache with write-back
and dirty-block flushing, and the measured behaviour under contention (the `lock.*` and `cache.*`
statistics keys listed in `stats.h`).
