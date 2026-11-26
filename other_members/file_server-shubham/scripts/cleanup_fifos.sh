#!/bin/bash
# Removes reply FIFOs left behind by clients that were killed with SIGKILL (they cannot clean up themselves).
for f in /tmp/fs_client_*; do
    [ -p "$f" ] || continue
    pid=${f##*_}
    [ -d "/proc/$pid" ] || { rm -f "$f"; echo "removed stale $f"; }
done
exit 0
