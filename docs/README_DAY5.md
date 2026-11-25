# Day 5 – Client & Tooling package

This is an additive package for the Shubham/client-tooling part of the OS project.
It adds repeatable shell smoke tests and a standalone formatter for key/value STATS output.

## Included

- `scripts/basic_ops.sh`: runs mkdir, create, ls, stats and delete through the CLI.
- `scripts/multi_client.sh`: launches N independent CLI processes concurrently.
- `scripts/format_stats.awk`: formats key/value STATS lines as a table.

## Important compatibility note

The exact server STATS response schema and the current `src/client/main.c` were not supplied
with this package. Therefore the AWK formatter is intentionally standalone; it is not wired
into the CLI automatically. It supports key/value lines separated by `:` or `=`. Do not replace
your existing `main.c` or protocol files blindly. Once the current repository source is available,
the formatter can be integrated against the actual STATS wire format without guessing.

## Run

Start the server in one terminal. In another terminal, from the repository root:

```bash
chmod +x scripts/basic_ops.sh scripts/multi_client.sh
./scripts/basic_ops.sh shubham
./scripts/multi_client.sh 4 shubham
```

Format a saved STATS response:

```bash
./bin/client shubham <<'COMMANDS' | awk -f scripts/format_stats.awk
stats
exit
COMMANDS
```

If your CLI prints a prompt or non-key/value text, the formatter ignores lines without `:` or `=`.

## Makefile

No Makefile change is required: these are directly executable shell/AWK scripts.
