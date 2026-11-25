#!/usr/bin/env bash
# Day 5 basic CLI operations smoke test.
# Usage: ./scripts/basic_ops.sh [username]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLIENT="${FS_CLIENT:-$ROOT_DIR/bin/client}"
USER_NAME="${1:-${FS_USER:-shubham}}"

if [[ ! -x "$CLIENT" ]]; then
  echo "ERROR: client not found or not executable: $CLIENT" >&2
  echo "Build it first with: make bin/client" >&2
  exit 1
fi

TMP_OUT="$(mktemp)"
trap 'rm -f "$TMP_OUT"' EXIT

# Keep each run isolated with a unique directory/file name.
TAG="day5_$$_${RANDOM}"
DIR="/$TAG"
FILE="$DIR/sample.txt"

{
  printf 'mkdir %s\n' "$DIR"
  printf 'create %s\n' "$FILE"
  printf 'ls %s\n' "$DIR"
  printf 'stats\n'
  printf 'delete %s\n' "$FILE"
  printf 'exit\n'
} | "$CLIENT" "$USER_NAME" >"$TMP_OUT" 2>&1 || {
  cat "$TMP_OUT"
  echo "ERROR: CLI returned a non-zero exit status." >&2
  exit 1
}

cat "$TMP_OUT"
if grep -Eiq 'cannot reach the server|unknown command|segmentation fault' "$TMP_OUT"; then
  echo "ERROR: basic operations test encountered a client/server error." >&2
  exit 1
fi

echo "BASIC OPS SCRIPT FINISHED"
