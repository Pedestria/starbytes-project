# Numerical Execution Improvement Plan

## Purpose

This plan targets the numerical bottlenecks exposed by Track A and separates three different problems that are currently mixed together:

1. Benchmark parity: several Starbytes Track A programs are not algorithmically equivalent to the Python versions.
2. Numeric correctness and representation: the runtime exposes `Float` and `Double` at the language level, but the runtime only stores `Int` and `Float`.
3. Hot-loop execution cost: numeric loops currently pay object boxing, refcount traffic, string-keyed local lookup, and generic array access on nearly every step.

The correct order is: fix parity, harden numeric representation, then optimize the execution path.

## Baseline

### Track A steady-state results

Source: `benchmark/results/summaries/track_a_steady-state_20260307T020520Z.report.md`

| workload | starbytes mean | python mean | relative to python |
|---|---:|---:|---:|
| binary-trees | 0.131803s | 0.408243s | 0.3229x |
| fasta | 0.176724s | 0.041856s | 4.2222x slower |
| k-nucleotide | 0.017569s | 0.040373s | 0.4352x |
| n-body | 0.440276s | 0.047931s | 9.1855x slower |
| regex-redux | 0.019853s | 0.045843s | 0.4331x |
| spectral-norm | 4.049372s | 0.097557s | 41.5078x slower |

The numerical hotspots are clear:

- `spectral-norm`
- `n-body`
- `fasta`

### Runtime share of the slow cases

Measured locally with `starbytes run --profile-compile ... -- <args>`:

| workload | total | runtime_exec | runtime share |
|---|---:|---:|---:|
| spectral-norm | 3967.093ms | 3960.710ms | 99.8% |
| n-body | 471.502ms | 461.175ms | 97.8% |
| fasta | 190.062ms | 183.000ms | 96.3% |

This means compiler-front-end work is not the main problem for these benchmarks. The plan has to improve runtime execution.

## Current Findings

### 1. Track A parity is not clean yet

The current Starbytes and Python programs are not equivalent in several cases.

#### binary-trees

- Python builds real trees and recursively checks them in `benchmark/languages/python/track_a/binary_trees.py`.
- Starbytes does arithmetic directly without allocating or walking trees in `benchmark/languages/starbytes/track_a/binary_trees.starb`.

Conclusion:

- Starbytes being faster here does not say much about runtime numerics.

#### spectral-norm

- Python performs the standard power-method structure with `multiply_av`, `multiply_atv`, and a final `math.sqrt(v_bv / vv)` in `benchmark/languages/python/track_a/spectral_norm.py`.
- Starbytes performs only one `A^T A` style pass per outer iteration and prints `vBv / vv` without the final square root in `benchmark/languages/starbytes/track_a/spectral_norm.starb`.

Conclusion:

- The Starbytes program does less work and computes a different final value, yet is still dramatically slower.
- That is a strong signal that the numeric execution path is currently expensive.

#### n-body

- Python uses the canonical inverse-distance-cubed update with `dist = math.sqrt(dist2)` and `mag = dt / (dist2 * dist)` in `benchmark/languages/python/track_a/n_body.py`.
- Python also offsets total momentum before advancing.
- Starbytes uses `dist2 + 0.000001`, skips the square root, and uses `mag = dt / (dist2 * dist2)` in `benchmark/languages/starbytes/track_a/n_body.starb`.

Conclusion:

- The Starbytes version is not the same algorithm, and it still loses badly.

#### fasta

- Python directly indexes `SOURCE[i % len(SOURCE)]` in `benchmark/languages/python/track_a/fasta.py`.
- Starbytes creates a one-character substring with `source.slice(sourceIndex, sourceIndex + 1)` in `benchmark/languages/starbytes/track_a/fasta.starb`.

Conclusion:

- This benchmark currently mixes numeric cost with avoidable string allocation and comparison cost.

### 2. Numeric representation is not ready for serious numerical work

Current runtime facts:

- `StarbytesNumT` only has `NumTypeFloat` and `NumTypeInt` in `include/starbytes/interop.h`.
- `StarbytesNumGetFloatValue` returns C `float`, not `double`, in `include/starbytes/interop.h`.
- `StarbytesNumNew`, `StarbytesNumCompare`, `StarbytesNumAdd`, and `StarbytesNumSub` store and compare through `float` in `src/runtime/RTObject.c`.
- Runtime type checks currently treat `Double` as `NumTypeFloat` in `src/runtime/RTEngine.cpp`.
- Runtime casts rewrite `Double` to `Float` in `src/runtime/RTEngine.cpp`.

Implications:

- `Double` is not a real runtime type today.
- Numerical benchmarks are running on a precision model that is weaker than the surface language suggests.
- Before optimization, Starbytes should define whether the numerical fast path is `Float`, `Double`, or both.

### 3. Hot loops are paying generic object costs

Current runtime facts:

- Binary arithmetic goes through boxed objects, type checks, conversion to generic numeric values, and boxed result construction in `src/runtime/RTEngine.cpp`.
- Array indexing does runtime object checks and increments element references on every read in `src/runtime/RTEngine.cpp`.
- `StarbytesArrayPush` reallocates on every push and stores boxed objects in `src/runtime/RTObject.c`.
- Variables are stored in `string_map<string_map<StarbytesObject>>` and looked up by scope name plus variable name in `src/runtime/RTEngine.cpp`.
- `referenceVariable` linearly scans the scope map entries and `memcmp`s names in `src/runtime/RTEngine.cpp`.

Implications:

- `spectral-norm` and `n-body` are currently paying for:
  - boxed numeric values
  - refcount churn
  - generic array/object dispatch
  - string-keyed local lookup inside inner loops

That is the wrong execution model for numerical kernels.

## Lessons From CPython

This project does not need to copy CPython exactly, but it should copy the principles that matter.

### CPython principle 1: specialize hot bytecode paths

PEP 659 describes the specializing adaptive interpreter used in CPython 3.11+:

- generic instructions start adaptive
- runtime type feedback specializes them
- specialized instructions handle common cases cheaply

The current CPython `Python/bytecodes.c` includes specialized and fused instructions such as:

- `BINARY_OP_ADD_FLOAT`
- `BINARY_OP_MULTIPLY_FLOAT`
- `BINARY_OP_SUBTRACT_FLOAT`
- `LOAD_FAST_LOAD_FAST`
- `LOAD_SMALL_INT`

Relevant sources:

- [PEP 659](https://peps.python.org/pep-0659/)
- [CPython bytecodes.c](https://github.com/python/cpython/blob/main/Python/bytecodes.c)

### CPython principle 2: keep numeric primitives cheap

CPython still uses objects, but it has optimized internal paths for common numeric cases:

- float operations are implemented in C in `Objects/floatobject.c`
- compact integer fast paths exist in `Objects/longobject.c`

Relevant sources:

- [CPython floatobject.c](https://github.com/python/cpython/blob/main/Objects/floatobject.c)
- [CPython longobject.c](https://github.com/python/cpython/blob/main/Objects/longobject.c)

### CPython principle 3: contiguous storage matters

Python benchmark code also relies on storage choices that reduce overhead:

- `n_body.py` uses `__slots__`
- list indexing stays on a contiguous array-backed container
- `fasta.py` indexes strings directly instead of creating substrings in the inner loop

Relevant sources:

- [CPython listobject.c](https://github.com/python/cpython/blob/main/Objects/listobject.c)
- local Track A Python sources under `benchmark/languages/python/track_a`

## Goals

The short-term goal is not "build a JIT". The short-term goal is:

1. Make the numerical benchmarks algorithmically correct.
2. Make `Double` real.
3. Remove obvious interpreter overhead from numeric loops.
4. Add specialization where the compiler already knows the types.

The longer-term goal is adaptive specialization for dynamic hot paths, but Starbytes should not wait for a full JIT before fixing the current bottlenecks.

## Implementation Plan

## Phase 0: Benchmark Parity Gate

Do this before claiming any speedup against Python.

### Work

- Replace `benchmark/languages/starbytes/track_a/binary_trees.starb` with a real tree-allocation benchmark or stop using it as a signal for numerical performance.
- Rewrite `benchmark/languages/starbytes/track_a/spectral_norm.starb` to match the Python algorithm:
  - same power-method structure
  - same number of `A^T A` applications
  - final `sqrt(vBv / vv)`
- Rewrite `benchmark/languages/starbytes/track_a/n_body.starb` to match the Python algorithm:
  - same constants
  - momentum offset
  - inverse-distance-cubed calculation using `sqrt`
- Decide whether Track A `fasta` is:
  - a simplified custom workload
  - or intended to track the common benchmark-game style workload
- Add output-equivalence checks between Starbytes and Python for Track A.

### Why first

Without parity, optimization work will be guided by mixed signals and can easily optimize the wrong thing.

## Phase 1: Numeric Representation and Correctness

This is the first real runtime/compiler phase.

### Work
- Add Lexer/Parser support for scientific notation. (for example `9.732e6` or `9.732e-6`)
- Add a real `NumTypeDouble`.
- Keep `Float` as 32-bit only if the language intends to preserve both types.
- Make `Double` use C `double` end-to-end:
  - runtime storage
  - casts
  - type tests
  - serialization
  - constant materialization
- Add `StarbytesNumGetDoubleValue` and `StarbytesNumGetFloatValue` with distinct semantics.
- Audit `Int`, `Long`, `Float`, and `Double` so runtime behavior matches semantic surface types.
- Add math intrinsics for:
  - `sqrt`
  - `abs`
  - `min`
  - `max`
- Lower those intrinsics directly to native C/C++ operations in the runtime.

### Why this phase matters

`spectral-norm` and `n-body` both need correct and fast floating-point math. Right now Starbytes does not have a trustworthy high-performance `Double` path.

## Phase 2: Slot-Based Locals

This is the most important interpreter optimization after numeric representation.

### Work

- Stop resolving local variables through `string_map<string_map<StarbytesObject>>`.
- During semantic/codegen, assign each local, argument, and temporary a frame slot index.
- Emit slot-based bytecode operands for:
  - load local
  - store local
  - load argument
  - store temporary
- Keep debug names separately for diagnostics and stack traces.
- Retain the current string-based path only for dynamic/global fallback cases.

### Why this phase matters

Current inner loops repeatedly look up names like `i`, `j`, `sum`, `dx`, `u`, and `mass` through scope maps and string comparisons. That is high overhead for every benchmarked numeric kernel.

### Expected impact

This should materially improve:

- `spectral-norm`
- `n-body`
- loop-heavy parts of `fasta`

even before typed arrays or adaptive specialization arrive.

## Phase 3: Typed Numeric Arrays

Starbytes currently stores array elements as boxed `StarbytesObject` values and grows arrays with `realloc` on every push.

### Work

- Add typed contiguous storage for:
  - `Array<Int>`
  - `Array<Double>`
  - optionally `Array<Float>`
- Introduce capacity growth instead of reallocating on every `push`.
- Add direct element access helpers that do not refcount per numeric element.
- Make `u[i]`, `v[i]`, `mass[i]`, `x[i]`, and similar numeric accesses lower to typed array get/set fast paths when the element type is known.
- Keep the generic boxed array path for mixed-type arrays.

### Why this phase matters

`spectral-norm` and `n-body` are array-heavy. Until numeric arrays stop moving boxed objects around, Starbytes will continue to lose to runtimes with contiguous numeric storage.

## Phase 4: Specialized Numeric Bytecode

Once locals and arrays have typed storage, the interpreter can execute typed math directly.

### Work

- Add specialized opcodes for common numeric kernels, for example:
  - `LOAD_SLOT_INT`
  - `LOAD_SLOT_DOUBLE`
  - `STORE_SLOT_INT`
  - `STORE_SLOT_DOUBLE`
  - `ADD_INT`
  - `ADD_DOUBLE`
  - `SUB_DOUBLE`
  - `MUL_DOUBLE`
  - `DIV_DOUBLE`
  - `NEG_DOUBLE`
  - `CMP_LT_INT`
  - `INDEX_GET_DOUBLE`
  - `INDEX_SET_DOUBLE`
  - `CALL_INTRINSIC_SQRT`
- Emit these directly at compile time when semantic analysis proves the types.
- Keep the current generic object opcode path as the fallback path.

### Why compile-time specialization first

Starbytes already has strong static type information for the Track A kernels. It should use that before building a more complex adaptive runtime layer.

## Phase 5: Superinstructions and Quickening

After compile-time specialization, add adaptive specialization for dynamic cases.

### Work

- Add adaptive opcodes that record operand kinds at runtime.
- Quickening targets should focus on:
  - binary numeric operators
  - local loads and stores
  - typed array indexing
  - intrinsic calls like `sqrt`
- Add superinstructions for common pairs and triples, for example:
  - `LOAD_SLOT_DOUBLE_LOAD_SLOT_DOUBLE`
  - `LOAD_SLOT_INT_CMP_LT`
  - `LOAD_SLOT_DOUBLE_INDEX_GET_DOUBLE`

### CPython parallel

This is the Starbytes analogue of the strategy described in PEP 659 and visible in CPython's specialized bytecode set.

## Phase 6: Workload-Specific Kernel Cleanup

This phase applies algorithm-aware cleanup after the execution model is improved.

### spectral-norm

- Match the Python algorithm exactly.
- Use `Double[]`, not boxed `Float[]`.
- Inline or lower `A(i, j)` aggressively.
- Hoist `n` and loop-invariant expressions.
- Use buffer swapping instead of copy loops where semantics permit.
- Consider an interpreter-recognized matrix-vector kernel only after generic typed-array execution is in place.

### n-body

- Match the Python algorithm exactly.
- Keep structure-of-arrays layout; it is a good choice for this workload.
- Use `Double[]` for all coordinate, velocity, and mass arrays.
- Add a `sqrt` intrinsic and compute inverse-distance cubed as:
  - `invDist = 1.0 / sqrt(dist2)`
  - `mag = dt * invDist * invDist * invDist`
- Hoist body count and constant masses.
- For the current five-body benchmark, optionally precompute pair indices or unroll the pair loop as a benchmark-specific optimization after the generic fast path lands.

### fasta

- Decide whether Track A `fasta` stays custom or moves toward the common benchmark definition.
- For the current custom workload:
  - stop using `slice()` for single-character extraction
  - use direct string indexing or byte indexing
  - map bases to integer codes
  - count in fixed-size integer storage
- If moving toward the common benchmark style:
  - use prefix probability tables
  - generate into output buffers in chunks
  - keep RNG and lookup on native numeric primitives

## Phase 7: Benchmark and Correctness Guardrails

### Add tests

- Numeric representation tests for `Float` vs `Double`
- cast and typecheck tests
- overflow and precision edge cases
- intrinsic accuracy tests for `sqrt`
- typed array correctness tests
- specialized-opcode fallback tests
- Track A output-equality tests against Python golden outputs

### Add benchmark gates

- rerun steady-state Track A after each phase
- report:
  - total time
  - runtime share
  - output parity
- do not accept a performance optimization that regresses numeric correctness

## Recommended Delivery Order

1. Phase 0: benchmark parity
2. Phase 1: real `Double` and math intrinsics
3. Phase 2: slot-based locals
4. Phase 3: typed numeric arrays
5. Phase 4: specialized numeric bytecode
6. Phase 6: workload-specific cleanup
7. Phase 5: adaptive quickening and superinstructions

That order is intentional:

- parity first prevents false wins
- representation first prevents optimizing the wrong numeric model
- slot-based locals and typed arrays remove the biggest interpreter costs
- specialized bytecode should be built on top of those foundations

## Initial Milestone

The smallest high-value vertical slice is:

1. Make `spectral-norm` algorithmically match Python.
2. Add real `Double`.
3. Add `sqrt`.
4. Add slot-based locals for numeric kernels.
5. Add `Double[]` specialized storage.
6. Re-run `spectral-norm`.

If that slice does not move `spectral-norm` substantially, Starbytes should pause before adding more specialization machinery and inspect where the remaining time is going.

## Sources

### Local benchmark data

- `benchmark/results/summaries/track_a_steady-state_20260307T020520Z.report.md`
- `benchmark/languages/starbytes/track_a/spectral_norm.starb`
- `benchmark/languages/starbytes/track_a/n_body.starb`
- `benchmark/languages/starbytes/track_a/fasta.starb`
- `benchmark/languages/starbytes/track_a/binary_trees.starb`
- `benchmark/languages/python/track_a/spectral_norm.py`
- `benchmark/languages/python/track_a/n_body.py`
- `benchmark/languages/python/track_a/fasta.py`
- `benchmark/languages/python/track_a/binary_trees.py`

### Local runtime/compiler sources

- `include/starbytes/interop.h`
- `src/runtime/RTObject.c`
- `src/runtime/RTEngine.cpp`

### External references

- [PEP 659: Specializing Adaptive Interpreter](https://peps.python.org/pep-0659/)
- [CPython Python/bytecodes.c](https://github.com/python/cpython/blob/main/Python/bytecodes.c)
- [CPython Objects/floatobject.c](https://github.com/python/cpython/blob/main/Objects/floatobject.c)
- [CPython Objects/longobject.c](https://github.com/python/cpython/blob/main/Objects/longobject.c)
- [CPython Objects/listobject.c](https://github.com/python/cpython/blob/main/Objects/listobject.c)
- [The Computer Language Benchmarks Game: n-body](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/nbody.html)
- [The Computer Language Benchmarks Game: spectral-norm](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/spectralnorm.html)
- [The Computer Language Benchmarks Game: fasta](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/fasta.html)
