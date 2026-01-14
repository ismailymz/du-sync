#!/usr/bin/env bash
set -euo pipefail

BIN="./du-sync"

tmp="$(mktemp -d)"
trap 'chmod -R u+rwX "$tmp" >/dev/null 2>&1 || true; rm -rf "$tmp"' EXIT

mkdir -p "$tmp/a/b"
printf "hello" > "$tmp/a/f1"       # 5
printf "world!!" > "$tmp/a/b/f2"   # 7

expected="$(
  find "$tmp" -type f -print0 \
    | du -b --files0-from=- -c \
    | tail -n1 | awk '{print $1}'
)"

got="$($BIN "$tmp" | awk '{print $1}')"

test "$got" = "$expected"
