#!/usr/bin/env bash
set -euo pipefail

BIN="./du-sync"

tmp="$(mktemp -d)"
trap 'chmod -R u+rwX "$tmp" >/dev/null 2>&1 || true; rm -rf "$tmp"' EXIT

mkdir -p "$tmp/dir"
printf "0123456789" > "$tmp/dir/orig"   # 10 bytes
ln "$tmp/dir/orig" "$tmp/dir/hardlink"  # same inode

expected="$(
  find "$tmp" -type f -print0 \
    | du -b --files0-from=- -c \
    | tail -n1 | awk '{print $1}'
)"

got="$($BIN "$tmp" | awk '{print $1}')"

test "$got" = "$expected"
