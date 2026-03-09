#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${STARBYTES_BUILD_DIR:-$ROOT_DIR/build}"
STARBYTES_BIN="$BUILD_DIR/bin/starbytes"
if [[ ! -x "$STARBYTES_BIN" && -x "${STARBYTES_BIN}.exe" ]]; then
  STARBYTES_BIN="${STARBYTES_BIN}.exe"
fi

rm -rf "$ROOT_DIR/.starbytes"
LOG_DIR="$ROOT_DIR/.starbytes/full-test-logs"
mkdir -p "$LOG_DIR"

if [[ ! -x "$STARBYTES_BIN" ]]; then
  echo "Missing binary: $STARBYTES_BIN"
  echo "Build first: cmake --build ${BUILD_DIR#$ROOT_DIR/} --target starbytes"
  exit 1
fi

PASS_COUNT=0
FAIL_COUNT=0

contains_pattern() {
  local pattern="$1"
  local file="$2"
  if command -v rg >/dev/null 2>&1; then
    rg -q -- "$pattern" "$file"
  else
    grep -q -- "$pattern" "$file"
  fi
}

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
  if contains_pattern "$pattern" "$log"; then
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
  if contains_pattern "$pattern" "$file"; then
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
run_expect_success "recursive-typed-locals-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/recursive_typed_locals_edge.starb"
run_expect_success "recursive-typed-locals-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/recursive_typed_locals_edge.starb"
assert_log_contains "recursive-typed-locals-run" "RECURSIVE-TYPED-LOCALS-OK"
run_expect_success "self-referential-class-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/self_referential_class_edge.starb"
run_expect_success "self-referential-class-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/self_referential_class_edge.starb"
assert_log_contains "self-referential-class-run" "SELF-REFERENTIAL-CLASS-OK"

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
run_expect_failure "typed-array-inferred-mismatch-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/typed_array_inferred_mismatch_invalid.starb"
assert_log_contains "typed-array-inferred-mismatch-check" 'Context: Type `Array` was implied from var initializer'
run_expect_failure "typed-nested-map-array-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/typed_nested_map_array_invalid.starb"
assert_log_contains "typed-nested-map-array-invalid-check" 'Context: Type `Array` was implied from var initializer'
run_expect_success "generic-inference-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_inference_extreme.starb"
run_expect_success "generic-inference-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_inference_extreme.starb"
assert_log_contains "generic-inference-run" "GENERIC-INFERENCE-OK"
run_expect_success "generic-edge-hardening-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_edge_hardening.starb"
run_expect_success "generic-edge-hardening-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_edge_hardening.starb"
assert_log_contains "generic-edge-hardening-run" "GENERIC-EDGE-HARDENING-OK"
run_expect_success "generic-free-function-phase1-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_free_function_phase1.starb"
run_expect_success "generic-free-function-phase1-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_free_function_phase1.starb"
assert_log_contains "generic-free-function-phase1-run" "GENERIC-FREE-FUNCTION-PHASE1-OK"
run_expect_success "generic-free-function-phase3-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_free_function_phase3.starb"
run_expect_success "generic-free-function-phase3-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_free_function_phase3.starb"
assert_log_contains "generic-free-function-phase3-run" "GENERIC-FREE-FUNCTION-PHASE3-OK"
run_expect_failure "generic-free-function-phase3-missing-args-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_free_function_phase3_missing_args_invalid.starb"
assert_log_contains "generic-free-function-phase3-missing-args-invalid-check" 'Could not infer generic type parameter `T` from invocation arguments'
run_expect_failure "generic-free-function-phase3-arity-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_free_function_phase3_arity_invalid.starb"
assert_log_contains "generic-free-function-phase3-arity-invalid-check" "Generic free-function invocation type argument count does not match function generic parameter count"
run_expect_failure "generic-free-function-phase3-arg-mismatch-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_free_function_phase3_arg_mismatch_invalid.starb"
assert_log_contains "generic-free-function-phase3-arg-mismatch-invalid-check" "Function argument type mismatch"
run_expect_failure "generic-free-function-phase3-unknown-type-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_free_function_phase3_unknown_type_invalid.starb"
assert_log_contains "generic-free-function-phase3-unknown-type-invalid-check" 'Unknown type `Missing`'
run_expect_success "generic-free-function-phase7-inference-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_free_function_phase7_inference.starb"
run_expect_success "generic-free-function-phase7-inference-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_free_function_phase7_inference.starb"
assert_log_contains "generic-free-function-phase7-inference-run" "GENERIC-FREE-FUNCTION-PHASE7-OK"
run_expect_failure "generic-free-function-phase7-conflict-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_free_function_phase7_conflict_invalid.starb"
assert_log_contains "generic-free-function-phase7-conflict-invalid-check" "Function argument type mismatch"
run_expect_success "generic-defaults-wave1-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_defaults_wave1.starb"
run_expect_success "generic-defaults-wave1-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_defaults_wave1.starb"
assert_log_contains "generic-defaults-wave1-run" "GENERIC-DEFAULTS-WAVE1-OK"
run_expect_failure "generic-defaults-non-trailing-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_defaults_non_trailing_invalid.starb"
assert_log_contains "generic-defaults-non-trailing-invalid-check" "Generic parameters with defaults must be trailing"
run_expect_failure "generic-defaults-unknown-type-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_defaults_unknown_type_invalid.starb"
assert_log_contains "generic-defaults-unknown-type-invalid-check" 'Unknown type `Missing`'
run_expect_failure "generic-defaults-missing-required-type-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_defaults_missing_required_type_invalid.starb"
assert_log_contains "generic-defaults-missing-required-type-invalid-check" 'Type `Pair` expects 2 type argument'
run_expect_success "generic-constructor-wave2-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_constructor_wave2.starb"
run_expect_success "generic-constructor-wave2-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_constructor_wave2.starb"
assert_log_contains "generic-constructor-wave2-run" "GENERIC-CONSTRUCTOR-WAVE2-OK"
run_expect_failure "generic-constructor-wave2-conflict-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_constructor_wave2_conflict_invalid.starb"
assert_log_contains "generic-constructor-wave2-conflict-invalid-check" 'Conflicting inferred types for generic parameter `U`'
run_expect_failure "generic-constructor-wave2-uninferable-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_constructor_wave2_uninferable_invalid.starb"
assert_log_contains "generic-constructor-wave2-uninferable-invalid-check" 'Could not infer generic type parameter `U` from invocation arguments'
run_expect_success "generic-methods-phase5-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_methods_phase5.starb"
run_expect_success "generic-methods-phase5-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_methods_phase5.starb"
assert_log_contains "generic-methods-phase5-run" "GENERIC-METHODS-PHASE5-OK"
run_expect_failure "generic-methods-phase5-missing-args-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_methods_phase5_missing_args_invalid.starb"
assert_log_contains "generic-methods-phase5-missing-args-invalid-check" 'Could not infer generic type parameter `U` from invocation arguments'
run_expect_failure "generic-methods-phase5-arity-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_methods_phase5_arity_invalid.starb"
assert_log_contains "generic-methods-phase5-arity-invalid-check" "Generic method invocation type argument count does not match method generic parameter count"
run_expect_failure "generic-methods-phase5-arg-mismatch-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_methods_phase5_arg_mismatch_invalid.starb"
assert_log_contains "generic-methods-phase5-arg-mismatch-invalid-check" "Method argument type mismatch"
run_expect_success "generic-methods-phase7-inference-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_methods_phase7_inference.starb"
run_expect_success "generic-methods-phase7-inference-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_methods_phase7_inference.starb"
assert_log_contains "generic-methods-phase7-inference-run" "GENERIC-METHODS-PHASE7-OK"
run_expect_failure "generic-methods-phase7-conflict-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_methods_phase7_conflict_invalid.starb"
assert_log_contains "generic-methods-phase7-conflict-invalid-check" "Method argument type mismatch"
run_expect_success "generic-interface-methods-phase6-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_interface_methods_phase6.starb"
run_expect_success "generic-interface-methods-phase6-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_interface_methods_phase6.starb"
assert_log_contains "generic-interface-methods-phase6-run" "GENERIC-INTERFACE-METHODS-PHASE6-OK"
run_expect_failure "generic-interface-methods-phase6-missing-args-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_interface_methods_phase6_missing_args_invalid.starb"
assert_log_contains "generic-interface-methods-phase6-missing-args-invalid-check" 'Could not infer generic type parameter `U` from invocation arguments'
run_expect_failure "generic-interface-methods-phase6-arity-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_interface_methods_phase6_invalid.starb"
assert_log_contains "generic-interface-methods-phase6-arity-invalid-check" "generic parameter count does not match interface method declaration"
run_expect_failure "generic-interface-methods-phase6-signature-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_interface_methods_phase6_impl_signature_invalid.starb"
assert_log_contains "generic-interface-methods-phase6-signature-invalid-check" "does not match interface parameter signature"
run_expect_success "generic-interface-methods-phase7-inference-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_interface_methods_phase7_inference.starb"
run_expect_success "generic-interface-methods-phase7-inference-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/generic_interface_methods_phase7_inference.starb"
assert_log_contains "generic-interface-methods-phase7-inference-run" "GENERIC-INTERFACE-METHODS-PHASE7-OK"
run_expect_failure "generic-edge-hardening-invalid-map-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_edge_hardening_invalid_map.starb"
assert_log_contains "generic-edge-hardening-invalid-map-check" 'Context: Type `Map` was implied from var initializer'
run_expect_failure "generic-edge-hardening-invalid-ternary-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/generic_edge_hardening_invalid_ternary.starb"
assert_log_contains "generic-edge-hardening-invalid-ternary-check" 'Context: Type `Array` was implied from var initializer'
run_expect_success "numeric-inference-default-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/numeric_inference_modes.starb"
assert_log_contains "numeric-inference-default-run" "INFER-32BIT-OK"
run_expect_success "numeric-inference-64bit-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/numeric_inference_modes.starb" --infer-64bit-numbers
assert_log_contains "numeric-inference-64bit-run" "INFER-64BIT-OK"
run_expect_success "numeric-phase1-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/numeric_phase1.starb"
run_expect_success "numeric-phase1-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/numeric_phase1.starb"
assert_log_contains "numeric-phase1-run" "NUMERIC-PHASE1-OK"
run_expect_success "slot-locals-phase2-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/slot_locals_phase2.starb"
run_expect_success "slot-locals-phase2-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/slot_locals_phase2.starb"
assert_log_contains "slot-locals-phase2-run" "SLOT-LOCALS-PHASE2-OK"
run_expect_success "numeric-arrays-phase3-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/numeric_arrays_phase3.starb"
run_expect_success "numeric-arrays-phase3-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/numeric_arrays_phase3.starb"
assert_log_contains "numeric-arrays-phase3-run" "NUMERIC-ARRAYS-PHASE3-OK"
run_expect_success "specialized-numeric-bytecode-phase4-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/specialized_numeric_bytecode_phase4.starb"
run_expect_success "specialized-numeric-bytecode-phase4-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/specialized_numeric_bytecode_phase4.starb"
assert_log_contains "specialized-numeric-bytecode-phase4-run" "NUMERIC-BYTECODE-PHASE4-OK"
run_expect_failure "math-intrinsics-invalid-runtime-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/math_intrinsics_invalid_runtime.starb"
assert_log_contains "math-intrinsics-invalid-runtime-run" "sqrt requires non-negative numeric input"
run_expect_success "inferred-var-types-custom-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/inferred_var_types_custom.starb"
assert_log_contains "inferred-var-types-custom-run" "INFER-CUSTOM-INVOKE-OK"
run_expect_success "map-long-double-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/map_long_double_support.starb"
run_expect_success "map-long-double-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/map_long_double_support.starb"
assert_log_contains "map-long-double-run" "MAP-LONG-DOUBLE-OK"
run_expect_success "math-module-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/math_module.starb"
run_expect_success "math-module-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/math_module.starb"
assert_log_contains "math-module-run" "MATH-MODULE-OK"
run_expect_success "math-module-phase3-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/math_module_phase3.starb"
run_expect_success "math-module-phase3-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/math_module_phase3.starb"
assert_log_contains "math-module-phase3-run" "MATH-MODULE-PHASE3-OK"
run_expect_success "math-module-phase2-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/math_module_phase2_invalid_runtime.starb"
run_expect_success "math-module-phase2-invalid-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/math_module_phase2_invalid_runtime.starb"
assert_log_contains "math-module-phase2-invalid-run" "log2 requires positive numeric input"
assert_log_contains "math-module-phase2-invalid-run" "approxEqual requires a non-negative numeric epsilon"
assert_log_contains "math-module-phase2-invalid-run" "clamp requires lower bound to be <= upper bound"
assert_log_contains "math-module-phase2-invalid-run" "gcd requires Int or Long arguments"
assert_log_contains "math-module-phase2-invalid-run" "nextPowerOfTwo requires a non-negative integer"
assert_log_contains "math-module-phase2-invalid-run" "MATH-MODULE-PHASE2-INVALID-OK"
run_expect_success "math-module-phase3-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/math_module_phase3_invalid_runtime.starb"
run_expect_success "math-module-phase3-invalid-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/math_module_phase3_invalid_runtime.starb"
assert_log_contains "math-module-phase3-invalid-run" "sum requires Array of numeric values"
assert_log_contains "math-module-phase3-invalid-run" "product requires Array of numeric values"
assert_log_contains "math-module-phase3-invalid-run" "mean requires a non-empty Array of numeric values"
assert_log_contains "math-module-phase3-invalid-run" "variance requires Array of numeric values"
assert_log_contains "math-module-phase3-invalid-run" "stddev requires a non-empty Array of numeric values"
assert_log_contains "math-module-phase3-invalid-run" "MATH-MODULE-PHASE3-INVALID-OK"
run_expect_success "regex-builtin-methods-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/regex_builtin_methods.starb"
run_expect_success "regex-builtin-methods-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/regex_builtin_methods.starb"
assert_log_contains "regex-builtin-methods-run" "REGEX-BUILTIN-METHODS-OK"
run_expect_failure "regex-builtin-methods-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/regex_builtin_methods_invalid.starb"
assert_log_contains "regex-builtin-methods-invalid-check" "Method argument type mismatch"
run_expect_failure "regex-text-module-removed-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/regex_text_module_removed_invalid.starb"
assert_log_contains "regex-text-module-removed-invalid-check" "Unknown symbol in scope member access"
run_expect_success "native-module-error-plumbing-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/native_module_error_plumbing.starb"
run_expect_success "native-module-error-plumbing-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/native_module_error_plumbing.starb"
assert_log_contains "native-module-error-plumbing-run" "pow requires numeric arguments"
assert_log_contains "native-module-error-plumbing-run" "log requires positive numeric input"
assert_log_contains "native-module-error-plumbing-run" "NATIVE-MODULE-ERROR-PLUMBING-OK"
run_expect_success "time-native-error-plumbing-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/time_native_error_plumbing.starb"
run_expect_success "time-native-error-plumbing-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/time_native_error_plumbing.starb"
assert_log_contains "time-native-error-plumbing-run" "timezoneFromID requires UTC, LOCAL, Z, or a numeric UTC offset"
assert_log_contains "time-native-error-plumbing-run" "parseISO8601 date/time components are out of range"
assert_log_contains "time-native-error-plumbing-run" "durationDiv requires Duration and finite non-zero divisor"
assert_log_contains "time-native-error-plumbing-run" "fromUnix nanos must be between 0 and 999999999"
assert_log_contains "time-native-error-plumbing-run" "TIME-NATIVE-ERROR-PLUMBING-OK"
run_expect_success "unicode-native-error-plumbing-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/unicode_native_error_plumbing.starb"
run_expect_success "unicode-native-error-plumbing-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/unicode_native_error_plumbing.starb"
assert_log_contains "unicode-native-error-plumbing-run" "normalize form must be one of NFC, NFD, NFKC, or NFKD"
assert_log_contains "unicode-native-error-plumbing-run" "localeFrom identifier must not be empty"
assert_log_contains "unicode-native-error-plumbing-run" "localeFrom identifier contains invalid characters"
assert_log_contains "unicode-native-error-plumbing-run" "breakIteratorCreate kind must be WORD, SENTENCE, LINE, or GRAPHEME"
assert_log_contains "unicode-native-error-plumbing-run" "fromCodepoints values must be supported Unicode scalar values"
assert_log_contains "unicode-native-error-plumbing-run" "UNICODE-NATIVE-ERROR-PLUMBING-OK"
run_expect_failure "map-type-mismatch-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/map_type_mismatch_invalid.starb"
assert_log_contains "map-type-mismatch-invalid-check" 'Context: Type `Map` was implied from var initializer'
run_expect_success "function-types-inline-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/function_types_inline.starb"
run_expect_success "function-types-inline-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/function_types_inline.starb"
assert_log_contains "function-types-inline-run" "FUNCTION-TYPES-INLINE-OK"
run_expect_failure "function-types-inline-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/function_types_inline_invalid.starb"
assert_log_contains "function-types-inline-invalid-check" 'Context: Type `String` was implied from return value'
run_expect_success "inline-function-literals-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/inline_function_literals.starb"
run_expect_success "inline-function-literals-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/inline_function_literals.starb"
assert_log_contains "inline-function-literals-run" "INLINE-FUNCTION-LITERALS-OK"
run_expect_failure "inline-function-literals-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/inline_function_literals_invalid.starb"
assert_log_contains "inline-function-literals-invalid-check" 'Context: Type `String` was implied from return value'
run_expect_success "function-hof-params-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/function_hof_params.starb"
run_expect_success "function-hof-params-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/function_hof_params.starb"
assert_log_contains "function-hof-params-run" "HOF-PARAMS-OK"
run_expect_success "typed-callable-collection-returns-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/typed_callable_collection_returns.starb"
run_expect_success "typed-callable-collection-returns-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/typed_callable_collection_returns.starb"
assert_log_contains "typed-callable-collection-returns-run" "TYPED-CALLABLE-COLLECTION-RETURNS-OK"
run_expect_failure "typed-callable-collection-returns-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/typed_callable_collection_returns_invalid.starb"
assert_log_contains "typed-callable-collection-returns-invalid-check" 'Context: Type `Map` was implied from return value'
assert_log_contains "typed-callable-collection-returns-invalid-check" 'Context: Type `Array` was implied from return value'
run_expect_success "cast-support-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/cast_support.starb"
run_expect_success "cast-support-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/cast_support.starb"
assert_log_contains "cast-support-run" "CAST-OK"
assert_log_contains "cast-support-run" "DOWNCAST-CATCH"
run_expect_failure "cast-invalid-unrelated-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/cast_invalid_unrelated.starb"
assert_log_contains "cast-invalid-unrelated-check" "Class cast source and target types are unrelated"
run_expect_failure "cast-downcast-unsecure-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/cast_downcast_unsecure.starb"
assert_log_contains "cast-downcast-unsecure-check" "Optional or throwable values must be captured with a secure declaration"

run_expect_success "stdlib-smoke-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/stdlib_smoke.starb"
run_expect_success "stdlib-smoke-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/stdlib_smoke.starb"
assert_log_contains "stdlib-smoke-run" "EXTREME-STDLIB-SMOKE-OK"
run_expect_failure "scoped-module-imports-flat-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/scoped_module_imports_flat_invalid.starb"
assert_log_contains "scoped-module-imports-flat-invalid-check" 'Imported symbol `timezoneUTC` must be referenced with its module name'
run_expect_success "scoped-module-imports-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/scoped_module_imports.starb"
run_expect_success "scoped-module-imports-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/scoped_module_imports.starb"
assert_log_contains "scoped-module-imports-run" "SCOPED-MODULE-IMPORTS-OK"
run_expect_success "secure-scope-hardening-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/secure_scope_hardening.starb"
run_expect_success "secure-scope-hardening-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/secure_scope_hardening.starb"
assert_log_contains "secure-scope-hardening-run" "SECURE-SCOPE-HARDENING-OK"
run_expect_success "secure-scope-hardening-nested-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/secure_scope_hardening_nested.starb"
run_expect_failure "secure-scope-hardening-invalid-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/secure_scope_hardening_invalid.starb"
assert_log_contains "secure-scope-hardening-invalid-check" 'Context: Type `Int` was implied from return value'
run_expect_success "cmdline-module-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/cmdline_module.starb" -- --verbose --config "./test.toml" --tag one --tag two positional
assert_log_contains "cmdline-module-run" "CMDLINE-MODULE-OK"

run_expect_success "module-app-check" "$STARBYTES_BIN" check "$ROOT_DIR/tests/extreme/modules/App"
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

TIME_NATIVE_MODULE="$BUILD_DIR/stdlib/libTime.ntstarbmod"
if [[ ! -f "$TIME_NATIVE_MODULE" && -f "$BUILD_DIR/stdlib/Time.ntstarbmod" ]]; then
  TIME_NATIVE_MODULE="$BUILD_DIR/stdlib/Time.ntstarbmod"
fi
run_expect_success "explicit-native-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/explicit_native.starb" --no-native-auto -n "$TIME_NATIVE_MODULE"
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

run_expect_success "io-stream-stability-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/io_stream_stability.starb"
assert_log_contains "io-stream-stability-run" "IO-STREAM-READABLE-OK"
assert_log_contains "io-stream-stability-run" "IO-STREAM-LINE-OK"
assert_log_contains "io-stream-stability-run" "IO-STREAM-EOF-CATCH"
assert_log_contains "io-stream-stability-run" "IO-STREAM-CLOSE-OK"
assert_log_contains "io-stream-stability-run" "IO-STREAM-RECLOSE-OK"
assert_log_contains "io-stream-stability-run" "IO-STREAM-AFTER-CLOSE-CATCH"
assert_log_contains "io-stream-stability-run" "IO-STREAM-CLOSED-OK"
assert_log_contains "io-stream-stability-run" "IO-STREAM-STABILITY-OK"
run_expect_success "unicode-advanced-stability-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/unicode_advanced_stability.starb"
assert_log_contains "unicode-advanced-stability-run" "UNICODE-FOLD-OK"
assert_log_contains "unicode-advanced-stability-run" "UNICODE-CONTAINS-OK"
assert_log_contains "unicode-advanced-stability-run" "UNICODE-CONTAINS-ROOT-OK"
assert_log_contains "unicode-advanced-stability-run" "UNICODE-ISNORM-BADFORM-CATCH"
assert_log_contains "unicode-advanced-stability-run" "UNICODE-LOCALE-BAD-CATCH"
assert_log_contains "unicode-advanced-stability-run" "UNICODE-ADVANCED-OK"
assert_log_contains "unicode-advanced-stability-run" "UNICODE-ADVANCED-STABILITY-OK"
run_expect_success "expected-phase3-invalid-runtime-run" "$STARBYTES_BIN" run "$ROOT_DIR/tests/extreme/expected_fail/phase3_invalid_runtime.starb"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-RANDOM-NEG-BYTES-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" "secureBytes count must be between 0 and 1048576"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-RANDOM-NEG-HEX-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" "secureHex byte count must be between 0 and 1048576"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-CRYPTO-BAD-SALT-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" "pbkdf2Sha256Hex saltHex must be valid hex"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-CRYPTO-BAD-ITER-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" "pbkdf2Sha256Hex iterations must be between 1 and 10000000"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-CRYPTO-BAD-HEX-BOOL-OK"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-COMP-BAD-HEX-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" "deflateHex inputHex must be valid hex"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-COMP-BAD-INFLATE-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" "inflateHex failed"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-COMP-BAD-GUNZIP-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" "gunzipText failed"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-ARCHIVE-BAD-PACK-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" 'packTextMapHex requires Dict<String,String> entries'
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-ARCHIVE-BAD-UNPACK-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" "unpackTextMapHex failed to decode archive"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-ARCHIVE-BAD-LIST-CATCH"
assert_log_contains "expected-phase3-invalid-runtime-run" "listEntries failed to decode archive"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-ARCHIVE-ISVALID-OK"
assert_log_contains "expected-phase3-invalid-runtime-run" "PH3-INVALID-CASE-OK"
run_expect_success "track-a-parity" python3 "$ROOT_DIR/benchmark/runners/check_track_a_parity.py" --starbytes-bin "$STARBYTES_BIN" --inputs "$ROOT_DIR/benchmark/data/track_a/track_a_parity_inputs.json"
assert_log_contains "track-a-parity" "TRACK-A-PARITY-OK"

echo
echo "Full test suite finished: pass=$PASS_COUNT fail=$FAIL_COUNT"
if [[ $FAIL_COUNT -ne 0 ]]; then
  exit 1
fi
