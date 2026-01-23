#!/usr/bin/env bash
set -euo pipefail

BIN="./du-sync"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/a/b/c/d/e/f/g"
printf "data" > "$tmp/a/b/c/d/e/f/g/file.txt"

expected="$(
  find "$tmp" -type f -print0 | du -b --files0-from=- -c | tail -n1 | awk '{print $1}'
)"

got="$($BIN "$tmp" | awk '{print $1}')"
test "$got" = "$expected"