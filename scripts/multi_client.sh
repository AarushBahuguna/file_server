#!/usr/bin/env bash
# Launch N independent CLI processes concurrently.
# Usage: ./scripts/multi_client.sh [clients] [username]
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
  echo "ERROR: client not found or not executable: $CLIENT" >&2
  exit 1
fi

TMP_DIR="$(mktemp -d)"
PIDS=()
cleanup() {
  for pid in "${PIDS[@]:-}"; do
    kill "$pid" 2>/dev/null || true
  done
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

for ((i=1; i<=COUNT; i++)); do
  OUT="$TMP_DIR/client_$i.log"
  (
    {
      printf 'pwd\n'
      printf 'ls\n'
      printf 'stats\n'
      printf 'exit\n'
    } | "$CLIENT" "${USER_NAME}_$i"
  ) >"$OUT" 2>&1 &
  PIDS+=("$!")
done

status=0
for i in "${!PIDS[@]}"; do
  if ! wait "${PIDS[$i]}"; then
    echo "Client $((i+1)) failed:" >&2
    cat "$TMP_DIR/client_$((i+1)).log" >&2
    status=1
  fi
done

for ((i=1; i<=COUNT; i++)); do
  echo "===== client $i ====="
  cat "$TMP_DIR/client_$i.log"
done

if (( status == 0 )); then
  echo "ALL $COUNT CLIENTS FINISHED"
fi
exit "$status"
