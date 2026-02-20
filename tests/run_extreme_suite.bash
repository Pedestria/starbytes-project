#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
STARBYTES_BIN="$ROOT_DIR/build/bin/starbytes"
LOG_DIR="$ROOT_DIR/.starbytes/extreme-logs"
mkdir -p "$LOG_DIR"

if [[ ! -x "$STARBYTES_BIN" ]]; then
  echo "Missing binary: $STARBYTES_BIN"
  echo "Build first: cmake --build build --target starbytes"
  exit 1
fi

PASS_COUNT=0
FAIL_COUNT=0

run_expect_success() {
  local name="$1"
  shift
  local log="$LOG_DIR/${name}.log"
  set +e
  "$@" >"$log" 2>&1
  local status=$?
  set -e
  if [[ $status -eq 0 ]]; then
    echo "[PASS] $name"
    PASS_COUNT=$((PASS_COUNT + 1))
  else
    echo "[FAIL] $name"
    echo "  command: $*"
    echo "  log: $log"
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi
}

run_expect_failure() {
  local name="$1"
  shift
  local log="$LOG_DIR/${name}.log"
  set +e
  "$@" >"$log" 2>&1
  local status=$?
  set -e
  if [[ $status -eq 0 ]]; then
    echo "[FAIL] $name (expected failure, got success)"
    echo "  command: $*"
    echo "  log: $log"
    FAIL_COUNT=$((FAIL_COUNT + 1))
  else
    echo "[PASS] $name (expected failure)"
    PASS_COUNT=$((PASS_COUNT + 1))
  fi
}

assert_log_contains() {
  local name="$1"
  local pattern="$2"
  local log="$LOG_DIR/${name}.log"
  if rg -q -- "$pattern" "$log"; then
    echo "[PASS] $name contains '$pattern'"
    PASS_COUNT=$((PASS_COUNT + 1))
  else
    echo "[FAIL] $name missing '$pattern'"
    echo "  log: $log"
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi
}

assert_file_contains() {
  local name="$1"
  local file="$2"
  local pattern="$3"
  if [[ ! -f "$file" ]]; then
    echo "[FAIL] $name missing file $file"
    FAIL_COUNT=$((FAIL_COUNT + 1))
    return
  fi
  if rg -q -- "$pattern" "$file"; then
    echo "[PASS] $name file contains '$pattern'"
    PASS_COUNT=$((PASS_COUNT + 1))
  else
    echo "[FAIL] $name file missing '$pattern'"
    echo "  file: $file"
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi
}

run_expect_success "core-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/core_extreme.starb"
run_expect_success "core-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/core_extreme.starb"
assert_log_contains "core-run" "EXTREME-CORE-OK"
assert_log_contains "core-run" "OPS-BITWISE-OK"
assert_log_contains "core-run" "OPS-SHORTCIRCUIT-OK"

run_expect_success "adt-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/adt_extreme.starb"
run_expect_success "adt-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/adt_extreme.starb"
assert_log_contains "adt-run" "EXTREME-ADT-OK"
run_expect_failure "adt-invalid-args-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/adt_invalid_args.starb"
assert_log_contains "adt-invalid-args-check" "Incorrect number of method arguments"
run_expect_success "typed-array-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/typed_array_syntax.starb"
run_expect_success "typed-array-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/typed_array_syntax.starb"
assert_log_contains "typed-array-run" "TYPED-ARRAY-SYNTAX-OK"
run_expect_failure "typed-array-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/typed_array_invalid.starb"
assert_log_contains "typed-array-invalid-check" "Method argument type mismatch"
run_expect_success "function-types-inline-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/function_types_inline.starb"
run_expect_success "function-types-inline-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/function_types_inline.starb"
assert_log_contains "function-types-inline-run" "FUNCTION-TYPES-INLINE-OK"
run_expect_failure "function-types-inline-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/function_types_inline_invalid.starb"
assert_log_contains "function-types-inline-invalid-check" "Inline function declared return type does not match implied return type"
run_expect_success "inline-function-literals-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/inline_function_literals.starb"
run_expect_success "inline-function-literals-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/inline_function_literals.starb"
assert_log_contains "inline-function-literals-run" "INLINE-FUNCTION-LITERALS-OK"
run_expect_failure "inline-function-literals-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/inline_function_literals_invalid.starb"
assert_log_contains "inline-function-literals-invalid-check" "Inline function declared return type does not match implied return type"
run_expect_success "function-hof-params-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/function_hof_params.starb"
run_expect_success "function-hof-params-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/function_hof_params.starb"
assert_log_contains "function-hof-params-run" "HOF-PARAMS-OK"

run_expect_success "stdlib-smoke-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/stdlib_smoke.starb"
run_expect_success "stdlib-smoke-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/stdlib_smoke.starb"
assert_log_contains "stdlib-smoke-run" "EXTREME-STDLIB-SMOKE-OK"

run_expect_success "module-app-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/modules/App"
assert_log_contains "module-app-run" "APP-MODULE-OK"

run_expect_success "modpath-app-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/modpath/App"
run_expect_success "modpath-app-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/modpath/App"
assert_log_contains "modpath-app-run" "MODPATH-OK"
assert_log_contains "modpath-app-run" "EXT-LIB-VALUE"

run_expect_failure "module-cycle-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/cycle/A"
assert_log_contains "module-cycle-check" "Cyclic module import detected"

run_expect_success "driver-help" "$STARBYTES_BIN" --help
run_expect_success "driver-version" "$STARBYTES_BIN" --version
run_expect_failure "driver-unknown-option" "$STARBYTES_BIN" --unknown
assert_log_contains "driver-unknown-option" "Unknown option"
run_expect_failure "driver-invalid-check-run-combo" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/cli/simple.starb" --run
assert_log_contains "driver-invalid-check-run-combo" "--run/--no-run are not valid with the check command"

STARBPKG_BIN="$ROOT_DIR/tools/starbpkg/starbpkg"
STARBPKG_PROJECT="$ROOT_DIR/.starbytes/extreme-starbpkg"
rm -rf "$STARBPKG_PROJECT"
mkdir -p "$STARBPKG_PROJECT"

run_expect_success "starbpkg-help" "$STARBPKG_BIN" --help
assert_log_contains "starbpkg-help" "Usage: starbpkg"

run_expect_success "starbpkg-init" "$STARBPKG_BIN" -C "$STARBPKG_PROJECT" init
assert_file_contains "starbpkg-init-modpaths" "$STARBPKG_PROJECT/.starbpkg/modpaths.txt" "\\./modules"
assert_file_contains "starbpkg-init-output" "$STARBPKG_PROJECT/.starbmodpath" "\\./stdlib"

run_expect_success "starbpkg-add-path" "$STARBPKG_BIN" -C "$STARBPKG_PROJECT" add-path "./vendor/lib"
assert_file_contains "starbpkg-add-path-output" "$STARBPKG_PROJECT/.starbmodpath" "\\./vendor/lib"

run_expect_success "starbpkg-status" "$STARBPKG_BIN" -C "$STARBPKG_PROJECT" status
assert_log_contains "starbpkg-status" "STARBPKG-STATUS"

run_expect_success "starbpkg-sync" "$STARBPKG_BIN" -C "$STARBPKG_PROJECT" sync

run_expect_success "driver-compile-clean" "$STARBYTES_BIN" compile "$ROOT_DIR/tests/extreme/cli/simple.starb" --out-dir "$ROOT_DIR/.starbytes/extreme-cli" --print-module-path --clean
assert_log_contains "driver-compile-clean" "simple.starbmod"
if [[ -f "$ROOT_DIR/.starbytes/extreme-cli/simple.starbmod" ]]; then
  echo "[FAIL] driver-compile-clean output module should be removed by --clean"
  FAIL_COUNT=$((FAIL_COUNT + 1))
else
  echo "[PASS] driver-compile-clean removed compiled module"
  PASS_COUNT=$((PASS_COUNT + 1))
fi
if [[ -f "$ROOT_DIR/.starbytes/extreme-cli/simple.starbsymtb" ]]; then
  echo "[FAIL] driver-compile-clean symbol table should be removed by --clean"
  FAIL_COUNT=$((FAIL_COUNT + 1))
else
  echo "[PASS] driver-compile-clean removed symbol table"
  PASS_COUNT=$((PASS_COUNT + 1))
fi
if [[ -f "$ROOT_DIR/.starbytes/extreme-cli/simple.starbint" ]]; then
  echo "[FAIL] driver-compile-clean interface file should be removed by --clean"
  FAIL_COUNT=$((FAIL_COUNT + 1))
else
  echo "[PASS] driver-compile-clean removed interface file (if generated)"
  PASS_COUNT=$((PASS_COUNT + 1))
fi
if [[ -d "$ROOT_DIR/.starbytes/extreme-cli/.cache" ]]; then
  echo "[FAIL] driver-compile-clean cache directory should be removed by --clean"
  FAIL_COUNT=$((FAIL_COUNT + 1))
else
  echo "[PASS] driver-compile-clean removed compile cache directory"
  PASS_COUNT=$((PASS_COUNT + 1))
fi

run_expect_success "no-native-auto-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/no_native_auto.starb" --no-native-auto
assert_log_contains "no-native-auto-run" "NO-NATIVE-CATCH"
assert_log_contains "no-native-auto-run" "native callback"

run_expect_success "explicit-native-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/explicit_native.starb" --no-native-auto -n "$ROOT_DIR/build/stdlib/libTime.ntstarbmod"
assert_log_contains "explicit-native-run" "UTC"

run_expect_failure "rttc-non-type" "$STARBYTES_BIN" check "$ROOT_DIR/tests/rttc_non_type_rhs.starb"
assert_log_contains "rttc-non-type" "Unknown type"
run_expect_failure "rttc-unknown-type" "$STARBYTES_BIN" check "$ROOT_DIR/tests/rttc_unknown_type.starb"
assert_log_contains "rttc-unknown-type" "Unknown type"
run_expect_failure "rttc-interface" "$STARBYTES_BIN" check "$ROOT_DIR/tests/rttc_interface_type.starb"
assert_log_contains "rttc-interface" "does not support interface types"

run_expect_success "ternary-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/ternary_support.starb"
run_expect_success "ternary-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/ternary_support.starb"
assert_log_contains "ternary-run" "TERNARY-OK"
run_expect_failure "ternary-invalid-condition" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/ternary_invalid_condition.starb"
assert_log_contains "ternary-invalid-condition" "Ternary condition must be Bool"
run_expect_failure "ternary-invalid-branches" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/ternary_invalid_branches.starb"
assert_log_contains "ternary-invalid-branches" "Ternary branch type mismatch"

run_expect_failure "operators-invalid-bitwise" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/operators_invalid.starb"
assert_log_contains "operators-invalid-bitwise" "Bitwise and shift operators require Int operands"

run_expect_success "expected-io-stream-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/expected_fail/io_stream_methods.starb"
run_expect_success "expected-unicode-advanced-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/expected_fail/unicode_advanced.starb"

echo
echo "Extreme suite finished: pass=$PASS_COUNT fail=$FAIL_COUNT"
if [[ $FAIL_COUNT -ne 0 ]]; then
  exit 1
fi
