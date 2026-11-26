#!/bin/bash
# Day 7: run the whole client-side test set against the REAL server and save the result.
#   ./scripts/integration_real.sh [server-binary]        (default ./bin/server)
# Write every failure into docs/bug_log.md (section "Integration bugs").
cd "$(dirname "$0")/.." || exit 1
SRV=${1:-./bin/server}
if [ ! -x "$SRV" ]; then
    echo "Real server not found: $SRV"
    echo "Build it first (ask Shwetank for the make target), or pass the path:  $0 <path>"
    exit 1
fi
make -s bin/client || exit 1
export SERVER_BIN="$SRV" EXPECT_LOCKS=1
mkdir -p docs/results
OUT=docs/results/integration_$(date +%Y%m%d_%H%M%S).txt
fails=0
for s in run_ops multi_rw stress; do
    echo; echo "################ $s  (server: $SRV)"
    ./scripts/$s.sh || { echo ">>> $s FAILED"; fails=$((fails + 1)); }
done 2>&1 | tee "$OUT"
echo
echo "saved: $OUT"
grep -q ">>> .* FAILED" "$OUT" && { echo "INTEGRATION HAS FAILURES - log them in docs/bug_log.md"; exit 1; }
echo "INTEGRATION PASSED"
