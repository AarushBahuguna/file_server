#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

cat <<'EOF'
=== Race scenario: two writers ===
No-lock mode: both writers can update the shared value concurrently.
Result: final content depends on interleaving and is not guaranteed to match either writer.

Locked mode: the first writer holds the lock and the second waits.
Result: writes execute serially and the final content is internally consistent.
EOF
