#!/usr/bin/env bash
# Build and run the boot-block helper test on the host (no ROM needed).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
G="$ROOT/libs/gearmulator/source"
OUT="${TMPDIR:-/tmp}/je8080_boot_helpers_test"
c++ -O1 -std=c++17 -I"$G/cpu" -I"$G" "$ROOT/tests/boot/boot_helpers_test.cpp" -o "$OUT"
"$OUT"
