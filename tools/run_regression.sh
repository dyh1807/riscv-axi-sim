#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

if command -v cmake >/dev/null 2>&1; then
  cmake -S . -B build
  cmake --build build -j8
  BIN="./build/single_cycle_axi4.out"
else
  echo "Warning: cmake not found, fallback to Makefile build" >&2
  make -j8
  BIN="./single_cycle_axi4.out"
fi

echo "[regression] dhrystone"
timeout 300s "$BIN" bin/dhrystone.bin

echo "[regression] coremark"
timeout 300s "$BIN" bin/coremark.bin

echo "[regression] linux"
timeout 700s "$BIN" bin/linux.bin
