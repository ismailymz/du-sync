#!/usr/bin/env bash
set -euo pipefail

BIN="./du-sync"

tmp="$(mktemp -d)"
trap 'chmod -R u+rwX "$tmp" >/dev/null 2>&1 || true; rm -rf "$tmp"' EXIT

mkdir -p "$tmp/ok"
printf "abc" > "$tmp/ok/f"  # 3

mkdir -p "$tmp/nope"
printf "secret" > "$tmp/nope/s" # 6
chmod 000 "$tmp/nope"

expected="$(
  find "$tmp" -type f -print0 2>/dev/null \
    | du -b --files0-from=- -c \
    | tail -n1 | awk '{print $1}'
)"

got="$($BIN "$tmp" 2>/dev/null | awk '{print $1}')"

test "$got" = "$expected"
