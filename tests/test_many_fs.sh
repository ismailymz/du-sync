#!/usr/bin/env bash
set -euo pipefail

BIN="./du-sync"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

for i in $(seq 1 500); do
    printf "x" > "$tmp/file_$i"
done

expected="$(
  find "$tmp" -type f -print0 | du -b --files0-from=- -c | tail -n1 | awk '{print $1}'
)"

got="$($BIN "$tmp" | awk '{print $1}')"
test "$got" = "$expected"