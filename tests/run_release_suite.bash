#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

echo "[release] Building all targets"
cmake --build "$BUILD_DIR" --target all

echo "[release] Running ctest"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "[release] Running full test suite"
bash "$ROOT_DIR/tests/run_full_test_suite.bash"

echo
echo "[release] Release suite passed"
