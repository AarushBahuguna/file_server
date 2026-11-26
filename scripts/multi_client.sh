#!/usr/bin/env bash
# Launch N independent CLI clients at the same time. Each one creates its own directory and file,
# writes, lists, deletes, and asks for stats. The script then CHECKS every client's output.
# A server must already be running (bin/fake_fs_server, bin/dummy_server or the real bin/server).
# Usage: ./scripts/multi_client.sh [clients: 1..32] [username]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLIENT="${FS_CLIENT:-$ROOT_DIR/bin/client}"
COUNT="${1:-4}"
USER_NAME="${2:-${FS_USER:-shubham}}"

if ! [[ "$COUNT" =~ ^[1-9][0-9]*$ ]] || (( COUNT > 32 )); then
  echo "Usage: $0 [clients: 1..32] [username]" >&2
  exit 2
fi
if [[ ! -x "$CLIENT" ]]; then
  echo "ERROR: client not found or not executable: $CLIENT (build it with: make bin/client)" >&2
  exit 1
fi

TMP_DIR="$(mktemp -d)"
PIDS=()
cleanup() {
  for pid in "${PIDS[@]:-}"; do
    [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
  done
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

for ((i=1; i<=COUNT; i++)); do
  d="/mc_$$_$i"
  (
    {
      printf 'mkdir %s\n' "$d"
      printf 'create %s/f.txt\n' "$d"
      printf 'open %s/f.txt w\n' "$d"          # first open in a session is fd 3
      printf 'write 3 0 hello\n'
      printf 'close 3\n'
      printf 'ls %s\n' "$d"
      printf 'rm %s/f.txt\n' "$d"
      printf 'rm %s\n' "$d"
      printf 'stats\n'
      printf 'exit\n'
    } | "$CLIENT" "${USER_NAME}_$i"
  ) >"$TMP_DIR/client_$i.log" 2>&1 &
  PIDS+=("$!")
done

status=0
for i in "${!PIDS[@]}"; do
  n=$((i + 1))
  if ! wait "${PIDS[$i]}"; then
    echo "FAIL: client $n exited with an error" >&2
    status=1
  fi
  if grep -qE '^(error:|login failed)' "$TMP_DIR/client_$n.log"; then
    echo "FAIL: client $n reported errors:" >&2
    grep -E '^(error:|login failed)' "$TMP_DIR/client_$n.log" | sed 's/^/    /' >&2
    status=1
  fi
  if ! grep -q 'wrote 5 bytes' "$TMP_DIR/client_$n.log"; then
    echo "FAIL: client $n: the write did not complete" >&2
    status=1
  fi
done

for ((i=1; i<=COUNT; i++)); do
  echo "===== client $i ====="
  cat "$TMP_DIR/client_$i.log"
done

if (( status == 0 )); then
  echo "ALL $COUNT CLIENTS FINISHED AND VERIFIED"
fi
exit "$status"
