# Runtime Profiling Implementation Plan

## Purpose

This document proposes a full runtime profiling implementation for Starbytes so runtime regressions can be localized quickly and compared across benchmark runs.

The immediate goal is not a generic observability platform. The immediate goal is:

1. identify which runtime subsystems dominate wall time on real workloads
2. localize regressions to specific opcode families, object operations, and functions
3. make Track A regressions reproducible and comparable across commits

This plan is motivated by the current slowdown signals in Track A, especially:

- `binary-trees` stalling or regressing in object-heavy recursive execution
- `spectral-norm` showing a major runtime regression
- `n-body`, `fasta`, and `k-nucleotide` also trending slower than the March 9 baseline

## Current State

Starbytes already has two useful profiling seams:

- compile profiling in [`tools/driver/main.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/main.cpp), [`tools/driver/profile/CompileProfile.h`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/profile/CompileProfile.h), and [`tools/driver/profile/CompileProfile.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/profile/CompileProfile.cpp)
- a small runtime profile surface in [`include/starbytes/runtime/RTEngine.h`](/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/runtime/RTEngine.h) and [`src/runtime/RTEngine.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp)

Today the runtime profile only exposes quickening counters:

- installed quickened sites
- quickened executions
- specializations
- fallbacks

That is useful, but it is too narrow to answer the main regression question:

- is time going to bytecode dispatch, object allocation, refcount churn, field lookup, array/string operations, native calls, or a specific function/opcode mix?

## Design Goals

1. Reuse the current driver/profile plumbing.
2. Keep profiling off by default and cheap when disabled.
3. Separate low-overhead aggregate profiling from higher-detail hotspot attribution.
4. Make output machine-readable first, human-readable second.
5. Preserve benchmark comparability by recording profiler mode and overhead-related metadata.
6. Avoid building a profiler that only explains one benchmark.

## What The Profiler Must Answer

For a given run, the profiler should answer:

1. How much total runtime was spent in the interpreter versus native/host work?
2. Which opcode families dominate execution count and time?
3. How much time and count do object operations contribute?
4. How much time is spent in class field/property access versus numeric work versus container work?
5. Which Starbytes functions or methods account for the most self time and inclusive time?
6. Where are quickened paths helping, and where are generic fallbacks still dominant?
7. Did the slowdown come from more operations, slower operations, or both?

## Profiling Model

The implementation should have two layers.

### Layer 1: Deterministic aggregate counters and timers

This is the default runtime profiling mode. It should provide stable totals suitable for A/B comparisons.

It records:

- wall-clock runtime totals
- bytecode dispatch counts by opcode
- time by opcode family
- object allocation and release counts
- time spent in key runtime subsystems
- function-level inclusive and self time
- call counts for interpreted and native calls

This layer is the main tool for benchmark regression triage.

### Layer 2: Optional hotspot attribution

This is a more detailed mode for deep diagnosis once Layer 1 identifies the suspicious subsystem.

Recommended implementation:

- sampled interpreter-state profiling based on periodic checks inside the dispatch loop

Each sample records:

- current function
- source file and line if available
- current opcode
- active subsystem tag

Reason for sampling:

- it avoids wrapping every tiny operation with a timer
- it produces hotspot rankings that are easier to read than raw counters alone
- it reduces profiler distortion on tight loops

This should be optional because it is more invasive and more complex than aggregate counters.

## Proposed Runtime Profile Surface

Expand `RuntimeProfileData` in [`include/starbytes/runtime/RTEngine.h`](/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/runtime/RTEngine.h) from a four-counter struct into a structured profile payload.

Recommended top-level sections:

- `metadata`
- `totals`
- `counts`
- `timings_ns`
- `opcode_stats`
- `function_stats`
- `subsystem_stats`
- `allocation_stats`
- `quickening_stats`
- `samples`

### Metadata

Record:

- profiler version
- profile mode: `aggregate` or `aggregate+sampling`
- input module path
- command: `run` or `compile --run`
- benchmark/workload label when provided
- success/failure
- runtime error string if any

### Totals

Record:

- total runtime nanoseconds
- interpreter dispatch loop nanoseconds
- native module load nanoseconds
- microtask drain nanoseconds
- sample count
- dispatch count
- function call count

### Counts

Record aggregate event counters for:

- object allocations by type
- object releases by type
- refcount increments and decrements
- property lookups, sets, and appends
- class slot gets and sets
- dict lookups and inserts
- array gets and sets
- string slice/index/concat operations
- regex matches/replacements
- native function calls
- runtime errors raised

### Timings

Record nanoseconds for coarse subsystems:

- `dispatch`
- `eval_expr`
- `invoke_func`
- `invoke_native`
- `member_get`
- `member_set`
- `index_get`
- `index_set`
- `new_object`
- `dict_ops`
- `array_ops`
- `string_ops`
- `regex_ops`
- `task_schedule`
- `microtasks`
- `refcount_ops`
- `alloc_ops`

These categories should be coarse enough to stay understandable and fine enough to isolate major regressions.

## Subsystem Taxonomy

The runtime should use a fixed subsystem enum rather than ad hoc string labels.

Recommended enum values:

- `Dispatch`
- `FunctionCall`
- `NativeCall`
- `ObjectAlloc`
- `RefCount`
- `MemberAccess`
- `ClassSlotAccess`
- `PropertyAccess`
- `ArrayOp`
- `DictOp`
- `StringOp`
- `RegexOp`
- `NumericOp`
- `Tasking`
- `Quickening`
- `ErrorHandling`

Why this matters:

- the JSON schema stays stable
- benchmark summarizers can compare profiles across commits
- the profiler avoids dynamic string-building in hot code

## Opcode Profiling

Per-opcode profiling is required because many regressions will present as:

- too many executions of a generic opcode
- a specialized opcode no longer being used
- a once-cheap opcode becoming slower

For each opcode, record:

- execution count
- total nanoseconds
- average nanoseconds
- quickened execution count where applicable
- fallback count where applicable

The profiler should also emit grouped opcode-family totals such as:

- locals and loads
- stores
- arithmetic
- comparisons
- calls
- member access
- indexing
- allocation/literals
- type checks/casts
- control flow

This lets reports stay readable even if the raw opcode table is large.

## Function-Level Profiling

Aggregate profiling should include a lightweight call tree summary keyed by runtime function template identity.

For each function or method:

- call count
- inclusive nanoseconds
- self nanoseconds
- object allocations performed inside the function
- top opcode family by time within the function

Implementation note:

- maintain a profiling call stack parallel to the existing local frame stack
- record entry timestamp on call
- charge child time separately so self time remains meaningful

This is the fastest path to seeing whether `binary-trees` is dominated by constructor recursion, tree checking, field access, or refcount churn.

## Sampling Design

Sampling should be opt-in and interpreter-owned.

Recommended approach:

1. keep a dispatch counter in `InterpImpl`
2. every `N` dispatches, check elapsed monotonic time
3. if the sample interval elapsed, append one sample record

Each sample should capture:

- current source file
- line number if recoverable
- current function name
- opcode
- subsystem tag

Recommended default:

- `1000us` sampling interval

Recommended non-goal:

- do not start with OS-thread signal-based sampling

Reason:

- it is harder to make portable
- it is unnecessary until the aggregate profiler proves insufficient

## Source and Bytecode Attribution

Good hotspot reports depend on mapping runtime execution back to source locations.

The profiler should therefore add or expose:

- current function template name during execution
- current bytecode offset
- optional source file and line mapping for runtime statements/expressions

If line-accurate mapping is not already emitted into runtime code, do this in two stages:

### Stage A

Record function name and opcode only.

### Stage B

Extend codegen to emit lightweight debug spans:

- bytecode offset
- source file id
- line

This is sufficient for hotspot reports without committing to a full debugger format.

## Allocation and Refcount Profiling

Current regressions strongly suggest object-lifetime costs may be material, especially after object model changes.

The runtime profiler should explicitly measure:

- allocation count by object kind
- bytes allocated where known
- inline-payload versus heap-payload object counts
- refcount increment/decrement counts
- release-triggered destructor counts
- property storage growth events
- array storage growth events

Implementation hooks belong mainly in:

- [`src/runtime/RTObject.c`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTObject.c)
- [`src/runtime/RTEngine.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp)

This is necessary to distinguish:

- more objects being created
- the same number of objects becoming more expensive
- the slowdown moving from allocation to lookup or dispatch

## Quickening and Specialization Profiling

The current quickening counters should remain, but they need more context.

Add:

- quickening attempts by opcode family
- successful specializations by opcode family
- fallback counts by reason
- nanoseconds spent in quickening installation
- nanoseconds spent on deoptimized/generic fallback execution

Fallback reasons should be categorized, for example:

- observed type mismatch
- unsupported class layout
- mixed boxed/unboxed values
- cache miss
- invalidated assumption

This will make it obvious when a slowdown is caused by hot code no longer staying on the specialized path.

## Driver and CLI Integration

Do not create a completely separate profiling command.

Extend the current driver in [`tools/driver/main.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/main.cpp) with runtime-specific options:

- `--profile-runtime`
- `--profile-runtime-out <file>`
- `--profile-runtime-sampling`
- `--profile-runtime-sample-us <int>`
- `--profile-runtime-top <int>`

Recommended behavior:

- `--profile-runtime` prints a runtime profile to stdout for `run` and `compile --run`
- `--profile-runtime-out` writes JSON to file
- `--profile-compile-out` may continue to embed the coarse `runtime_exec` summary, but detailed runtime profile data should live in the dedicated runtime profile output

Why split outputs:

- compile profile consumers already exist
- runtime profile data will be much larger
- keeping schemas separate avoids breaking compile-profile tooling

## Output Format

Primary output should be JSON.

Recommended top-level schema:

```json
{
  "type": "runtime_profile",
  "version": 1,
  "mode": "aggregate+sampling",
  "command": "compile",
  "success": true,
  "input": "/abs/path/program.starb",
  "module": "program",
  "timings_ns": {},
  "counts": {},
  "quickening": {},
  "subsystems": [],
  "opcodes": [],
  "functions": [],
  "samples": []
}
```

Human-readable reporting should be a separate summarizer, not hand-built in the runtime.

Add a summarizer script under `benchmark/runners/` or `tools/driver/profile/` that prints:

- top subsystems by time
- top opcodes by time
- top functions by self and inclusive time
- top allocation-heavy object kinds
- quickening success/fallback summary

## Benchmark Harness Integration

Track A should be able to collect runtime profiles per workload.

Add support to [`benchmark/runners/run_track_a.py`](/Users/alextopper/Documents/GitHub/starbytes-project/benchmark/runners/run_track_a.py) for an optional profiling mode:

- one smoke run per workload with `--profile-runtime-out`
- store raw runtime profile JSON next to the hyperfine output

Recommended directory layout:

```text
benchmark/results/raw/<run_id>/
  steady-state_binary-trees.hyperfine.json
  steady-state_binary-trees.runtime-profile.json
  ...
```

This should not replace timing runs. It should complement them.

Reason:

- `hyperfine` gives statistically useful runtime comparisons
- runtime profiling gives causal diagnostics

## Implementation Phases

## Phase 0: Plumbing and Schema

### Work

- Expand `RuntimeProfileData` to hold structured counters and timings.
- Add a runtime-profile JSON writer beside the existing compile-profile writer.
- Add CLI flags for runtime profile output.
- Thread the enabled/disabled profile state through `InterpImpl`.

### Acceptance

- `starbytes run ... --profile-runtime-out file.json` emits a valid runtime profile
- disabled profiling adds near-zero overhead beyond a branch check

## Phase 1: Coarse Runtime Timers

### Work

- Add aggregate timers around major runtime subsystems
- Add dispatch and call counters
- Add per-opcode execution counts
- Add per-function call counts and inclusive timing

### Acceptance

- Track A runs can show a top-level time split such as dispatch versus member access versus allocation

## Phase 2: Object and Container Profiling

### Work

- Instrument `RTObject.c` allocation, release, refcount, property, array, dict, and string operation counts
- Attribute these events to the active function/subsystem

### Acceptance

- `binary-trees` and `fasta` profiles clearly show whether object churn or string/container operations dominate

## Phase 3: Self Time and Opcode Timing

### Work

- Add self-time accounting for functions
- Add per-opcode total nanoseconds
- Group opcodes into stable families for report output

### Acceptance

- regressions can be localized to a small number of opcodes or functions instead of only broad subsystems

## Phase 4: Sampling Hotspots

### Work

- Add opt-in sampled interpreter-state profiling
- Record current function, opcode, subsystem, and source span if available
- Add hotspot summarizer output

### Acceptance

- long-running workloads produce ranked hotspot samples that match the aggregate profile story

## Phase 5: Benchmark Correlation

### Work

- Add Track A runtime-profile collection mode
- Add a comparison script for two runtime profiles
- Emit delta tables by subsystem, opcode family, and function

### Acceptance

- a regression report can say not only that `spectral-norm` became slower, but whether the delta came from numeric dispatch, fallback specialization, allocation churn, or a specific function

## Validation Plan

Validation should include both correctness and profiler-quality checks.

### Correctness checks

- profiling enabled must not change program output
- profiling enabled must not suppress runtime diagnostics
- compile profile behavior must remain intact

### Profiler sanity checks

- total runtime in runtime profile should approximately match `runtime_exec` from compile profile
- summed subsystem time should be close to total runtime, with clearly documented overlap rules
- summed function inclusive time should exceed total runtime only in expected parent/child ways
- sampled hotspots should broadly agree with aggregate subsystem and opcode totals

### Overhead checks

Measure profiler overhead on at least:

- `binary-trees`
- `n-body`
- `spectral-norm`
- `fasta`

Target:

- aggregate mode overhead low enough for routine diagnostic use
- sampling mode overhead acceptable for deep diagnosis, even if higher

## Important Implementation Rules

1. Do not time every tiny helper with separate `steady_clock` calls unless that helper is already known to be coarse enough.
2. Prefer RAII timing scopes around subsystem-sized regions.
3. Use fixed enums and integer ids in hot paths, not dynamic strings.
4. Make all expensive profile detail conditional on runtime flags.
5. Keep JSON emission out of the hot path; only serialize after execution finishes.

## Recommended First Slice

The smallest meaningful vertical slice is:

1. add `--profile-runtime-out`
2. record total runtime, dispatch count, per-opcode counts, coarse subsystem timings, and per-function call counts
3. add Track A profile capture for one run per workload
4. add a small summarizer that prints top subsystems and top functions

That slice is enough to answer the current slowdown question without waiting for line-accurate sampling.

## Expected Value

If implemented, this profiler should let Starbytes answer questions like:

- `binary-trees` is 4x slower because object allocation and member access now dominate self time in `Node.check`
- `spectral-norm` regressed because typed numeric quickening stopped sticking and generic arithmetic opcodes now dominate
- `fasta` is spending most time in string slicing and tiny object churn rather than numeric work

That is the level of visibility needed before doing another broad optimization pass.
