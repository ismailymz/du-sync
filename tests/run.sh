#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

fail=0
for t in "$DIR"/test_*.sh; do
  echo "==> $t"
  if ! "$t"; then
    echo "FAILED: $t" >&2
    fail=1
  fi
done

exit "$fail"
