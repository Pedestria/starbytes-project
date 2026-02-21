# Compile Time Benchmark: Phase 3 and 4

Date: February 20, 2026  
Project: Starbytes  
Scope: Compile-time optimization comparison for Phase 3 (symbol resolution indexing) and Phase 4 (allocation path consolidation)

## Benchmark Setup

- Binary: `/Users/alextopper/Documents/GitHub/starbytes-project/build/bin/starbytes`
- Input source: `/Users/alextopper/Documents/GitHub/starbytes-project/tests/test.starb`
- Runs per mode: 8 (after warm-up)
- Profile flag: `--profile-compile-out <file>`

Modes:
- Legacy baseline: `STARBYTES_DISABLE_SYMBOL_INDEX=1` (linear symbol lookup path)
- Optimized: default behavior (indexed symbol lookup path)

## Results: `check`

Average timings (ms):

| Metric | Legacy | Optimized | Improvement |
|---|---:|---:|---:|
| total | 6.278 | 3.527 | 43.83% |
| parse_total | 5.450 | 2.675 | 50.92% |
| semantic | 4.359 | 1.625 | 62.73% |

Raw profiles directory:
- `/tmp/starbytes_profile_compare`

## Results: `compile`

Average timings (ms):

| Metric | Legacy | Optimized | Improvement |
|---|---:|---:|---:|
| total | 6.659 | 4.069 | 38.90% |
| parse_total | 5.430 | 2.811 | 48.23% |
| semantic | 4.185 | 1.593 | 61.94% |
| gen_finish | 0.152 | 0.137 | 9.88% |

Raw profiles directory:
- `/tmp/starbytes_profile_compare_compile`

## Interpretation

- The largest gain is in semantic analysis, matching the intent of Phase 3.
- Overall compile and check latency both improved substantially.
- Codegen finalization (`gen_finish`) changed only slightly, as expected (optimizations were not primarily codegen-focused).

## Repro Commands

```bash
BIN=/Users/alextopper/Documents/GitHub/starbytes-project/build/bin/starbytes
SRC=/Users/alextopper/Documents/GitHub/starbytes-project/tests/test.starb

# Warm-up
STARBYTES_DISABLE_SYMBOL_INDEX=1 "$BIN" check "$SRC" --profile-compile-out /tmp/starbytes_profile_compare/warm_legacy.json
"$BIN" check "$SRC" --profile-compile-out /tmp/starbytes_profile_compare/warm_opt.json

# Measured runs (check)
for i in 1 2 3 4 5 6 7 8; do
  STARBYTES_DISABLE_SYMBOL_INDEX=1 "$BIN" check "$SRC" --profile-compile-out "/tmp/starbytes_profile_compare/legacy_${i}.json"
  "$BIN" check "$SRC" --profile-compile-out "/tmp/starbytes_profile_compare/opt_${i}.json"
done

# Warm-up
STARBYTES_DISABLE_SYMBOL_INDEX=1 "$BIN" compile "$SRC" -o /tmp/starbytes_profile_compare_compile/warm_legacy.starbmod --profile-compile-out /tmp/starbytes_profile_compare_compile/warm_legacy.json
"$BIN" compile "$SRC" -o /tmp/starbytes_profile_compare_compile/warm_opt.starbmod --profile-compile-out /tmp/starbytes_profile_compare_compile/warm_opt.json

# Measured runs (compile)
for i in 1 2 3 4 5 6 7 8; do
  STARBYTES_DISABLE_SYMBOL_INDEX=1 "$BIN" compile "$SRC" -o "/tmp/starbytes_profile_compare_compile/legacy_${i}.starbmod" --profile-compile-out "/tmp/starbytes_profile_compare_compile/legacy_${i}.json"
  "$BIN" compile "$SRC" -o "/tmp/starbytes_profile_compare_compile/opt_${i}.starbmod" --profile-compile-out "/tmp/starbytes_profile_compare_compile/opt_${i}.json"
done
```

