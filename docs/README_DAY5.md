# Day 5 - statistics formatting and ops scripts

- `include/stats.h`, `src/common/stats.c` - the stats text format (`key value` lines), server-side
  `stats_put_*` helpers, and the CLI renderer: `stats` (table), `stats raw`, `stats csv`.
  The key list per team member is at the top of `stats.h`.
- `tests/fake_fs_server.c` - in-memory server speaking the real protocol, so the CLI and scripts can be
  tested before the real server exists.
- `scripts/ops_basic.sh`, `ops_dirs.sh`, `ops_errors.sh`, `ops_stats.sh`, `run_ops.sh` - self-checking scripts.
  Against the real server:  `SERVER_BIN=./bin/server ./scripts/run_ops.sh`
- `scripts/multi_client.sh` - N concurrent clients doing real work against an already running server; verified.

Run: `make stats-test`, `make ops-test`, or `make check` for everything.
