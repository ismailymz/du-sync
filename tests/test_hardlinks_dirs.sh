#!/usr/bin/env bash
set -euo pipefail

BIN="./du-sync"

tmp="$(mktemp -d)"
trap 'chmod -R u+rwX "$tmp" >/dev/null 2>&1 || true; rm -rf "$tmp"' EXIT

mkdir -p "$tmp/dir1" "$tmp/dir2"

printf "0123456789abcdef" > "$tmp/dir1/orig"
ln "$tmp/dir1/orig" "$tmp/dir2/hardlink"

expected="$(
  find "$tmp" -type f -print0 \
    | du -b --files0-from=- -c \
    | tail -n1 | awk '{print $1}'
)"

got="$($BIN "$tmp" | awk '{print $1}')"

test "$got" = "$expected"