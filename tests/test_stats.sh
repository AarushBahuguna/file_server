#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AWK_FILE="$ROOT_DIR/scripts/format_stats.awk"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

if [[ ! -f "$AWK_FILE" ]]; then
    echo "FAIL: formatter not found"
    exit 1
fi

printf 'cache_hits: 150\ncache_misses: 25\nScheduler policy: SCAN\nrequests=200\n' |
    awk -f "$AWK_FILE" > "$TMP_DIR/normal.out"

grep -q 'cache_hits' "$TMP_DIR/normal.out"
grep -q '150' "$TMP_DIR/normal.out"
grep -q 'Scheduler policy' "$TMP_DIR/normal.out"
grep -q 'SCAN' "$TMP_DIR/normal.out"
grep -q 'requests' "$TMP_DIR/normal.out"

echo "PASS: normal key/value formats"

printf '' | awk -f "$AWK_FILE" > "$TMP_DIR/empty.out"

grep -q 'STATISTIC' "$TMP_DIR/empty.out"

if grep -q 'cache_hits' "$TMP_DIR/empty.out"; then
    echo "FAIL: empty input produced unexpected data"
    exit 1
fi

echo "PASS: empty input"

printf 'total requests: 500\nactive_sessions=4\ninvalid line\n' |
    awk -f "$AWK_FILE" > "$TMP_DIR/mixed.out"

grep -Eq 'total requests[[:space:]]+\|[[:space:]]*500' "$TMP_DIR/mixed.out"
grep -Eq 'active_sessions[[:space:]]+\|[[:space:]]*4' "$TMP_DIR/mixed.out"

if grep -q 'invalid line' "$TMP_DIR/mixed.out"; then
    echo "FAIL: malformed line was not ignored"
    exit 1
fi

echo "PASS: malformed and mixed input"
echo "ALL STATS TESTS PASSED"
