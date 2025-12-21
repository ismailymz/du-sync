#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BIN="${ROOT_DIR}/du-sync"

if [[ ! -x "${BIN}" ]]; then
  echo "[TEST] Binary not found. Building..."
  (cd "${ROOT_DIR}" && make clean && make)
fi

run_case () {
  local path="$1"
  echo "----------------------------------------"
  echo "[TEST] Path: ${path}"

  local ours
  ours="$("${BIN}" "${path}" | awk '{print $1}')"

  local ref
  ref="$(du -sb "${path}" | awk '{print $1}')"

  echo "[TEST] ours=${ours}  du=${ref}"

  if [[ "${ours}" != "${ref}" ]]; then
    echo "[FAIL] Mismatch for ${path}"
    exit 1
  fi

  echo "[PASS]"
}

echo "[TEST] Running du-sync tests..."

run_case "${ROOT_DIR}"          # proje klasörü
run_case "${ROOT_DIR}/src"      # src altı
run_case "${ROOT_DIR}/include"  # include altı

# inode test: hard link double counting
TMPDIR="$(mktemp -d)"
echo "hello" > "${TMPDIR}/a.txt"
ln "${TMPDIR}/a.txt" "${TMPDIR}/b.txt"
run_case "${TMPDIR}"
rm -rf "${TMPDIR}"

echo "----------------------------------------"
echo "[ALL PASS] ✅"
