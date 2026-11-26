#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

HOST=${FS_SERVER_HOST:-127.0.0.1}
PORT=${FS_SERVER_PORT:-9000}

printf 'TCP multi-laptop smoke test\n'
printf 'Target: %s:%s\n\n' "$HOST" "$PORT"

printf 'stats\nexit\n' | FS_TRANSPORT=tcp FS_SERVER_HOST="$HOST" FS_SERVER_PORT="$PORT" ./client "alice-$HOST"

printf 'create network-smoke.txt\nopen network-smoke.txt w\nwrite 3 0 hello-from-network-client\nclose 3\nopen network-smoke.txt r\nread 3 0 64\nclose 3\ndelete network-smoke.txt\nexit\n' | FS_TRANSPORT=tcp FS_SERVER_HOST="$HOST" FS_SERVER_PORT="$PORT" ./client "bob-$HOST"

echo "TCP smoke test completed against $HOST:$PORT"
