# Runtime Profiling Implementation Plan

## Purpose

This document tracks the runtime profiling work for Starbytes.

It is no longer a pure proposal. A meaningful runtime profiling surface is already implemented, and this document now distinguishes:

- what exists today
- what is still missing for deeper regression diagnosis
- what the next implementation slices should be

The immediate goal remains:

1. identify which runtime subsystems dominate wall time on real workloads
2. localize regressions to specific opcodes, functions, and runtime domains
3. make Track A regressions reproducible and comparable across commits

## Current Track A Motivation

Recent Track A data still shows that runtime visibility matters:

- `binary-trees` is heavily dominated by recursive function execution, member access, and allocation work
- `spectral-norm` shows a major runtime-side slowdown centered on function execution
- `n-body`, `fasta`, and `k-nucleotide` also benefit from subsystem and opcode visibility

That is the context for the current profiling surface and the remaining plan.

## Implemented Today

## Runtime Profile Surface

The runtime profile surface in [`include/starbytes/runtime/RTEngine.h`](/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/runtime/RTEngine.h) and [`src/runtime/RTEngine.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp) already records:

- `enabled`
- `totalRuntimeNs`
- `dispatchCount`
- `functionCallCount`
- per-opcode execution counts
- subsystem timing totals
- object allocation counts by kind
- object deallocation counts by kind
- refcount increment/decrement counts
- per-function call count
- per-function inclusive time
- per-function self time
- quickening counters:
  - installed sites
  - executions
  - specializations
  - fallbacks

The current fixed subsystem taxonomy is:

- `FunctionCall`
- `NativeCall`
- `MemberAccess`
- `IndexAccess`
- `ObjectAllocation`
- `Microtasks`

This is a real aggregate profiler now, not just a quickening counter surface.

## Driver and CLI Integration

The driver integration in [`tools/driver/main.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/main.cpp) is already implemented.

Shipped flags:

- `--profile-runtime`
- `--profile-runtime-out <path>`

Current behavior:

- `--profile-runtime` prints a human-readable runtime summary after `run` or `compile --run`
- `--profile-runtime-out` writes runtime profile JSON
- runtime profiling can be enabled independently of compile profiling
- compile profiling still embeds coarse runtime execution time and quickening counters in the compile profile output

CLI parsing for these flags is covered in [`tests/CmdLineParserTest.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/tests/CmdLineParserTest.cpp).

## JSON and Summary Output

Runtime profile JSON and summary output are implemented in:

- [`tools/driver/profile/RuntimeProfile.h`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/profile/RuntimeProfile.h)
- [`tools/driver/profile/RuntimeProfile.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/profile/RuntimeProfile.cpp)

The current JSON output includes:

- `type`
- `version`
- `command`
- `success`
- `input`
- `module`
- `runtime_error`
- `overlap_rules`
- `counts`
- `objects`
- `timings_ms.total_runtime`
- `subsystems[]`
- `opcodes[]`
- `functions[]`

The current summary output prints:

- total runtime
- dispatch count
- function call count
- refcount counts
- overlap notes for subsystem and function totals
- top subsystems by time
- top functions by time
- object allocation/deallocation counts by kind

## Track A Harness Integration

Track A runtime-profile capture is also already implemented in:

- [`benchmark/runners/run_track_a.py`](/Users/alextopper/Documents/GitHub/starbytes-project/benchmark/runners/run_track_a.py)

Shipped harness support:

- `--starbytes-runtime-profiles`
- one runtime-profile capture per workload
- raw profile JSON stored next to hyperfine output

This produces the directory shape the original plan asked for:

```text
benchmark/results/raw/<run_id>/
  steady-state_binary-trees.hyperfine.json
  steady-state_binary-trees.runtime-profile.json
  ...
```

## Tests

Runtime profiling has dedicated validation in:

- [`tests/RuntimeProfileTest.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/tests/RuntimeProfileTest.cpp)

That test already verifies:

- profiling can be enabled
- total runtime and dispatch count are populated
- object allocation and member-invoke opcode counts are populated
- subsystem timers are populated for function calls, member access, and object allocation
- function-level profile entries are populated

## Current Runtime Profile Semantics

The current profiler is best described as:

- deterministic aggregate profiling
- always interpreter-owned
- low-detail, low-schema-complexity
- useful for ranking hotspots
- not yet a full execution accounting system

What it answers well today:

1. total runtime for the interpreted portion of a run
2. which opcodes execute most often
3. which coarse subsystems dominate
4. which object kinds are being allocated and deallocated
5. whether refcount churn is high at a coarse aggregate level
6. which Starbytes functions dominate inclusive and self time
7. whether quickening is active and sticking

What it does not answer well yet:

1. exact time by opcode
2. bytes allocated
3. source line attribution
4. fallback reasons for specialization failure
5. exclusive subsystem percentages
6. benchmark-integrated differential reporting

## Current JSON Shape

The implemented JSON shape is currently closer to:

```json
{
  "type": "runtime_profile",
  "version": 1,
  "command": "compile",
  "success": true,
  "input": "/abs/path/program.starb",
  "module": "program",
  "runtime_error": "",
  "overlap_rules": {
    "subsystems_are_inclusive": true,
    "function_totals_are_inclusive": true
  },
  "counts": {
    "dispatch": 123,
    "function_calls": 4,
    "refcount_increments": 200,
    "refcount_decrements": 198,
    "runtime_quickened_sites": 0,
    "runtime_quickened_executions": 0,
    "runtime_quickened_specializations": 0,
    "runtime_quickened_fallbacks": 0
  },
  "objects": {
    "allocations": [
      {"kind": "custom_class", "count": 1}
    ],
    "deallocations": [
      {"kind": "custom_class", "count": 1}
    ]
  },
  "timings_ms": {
    "total_runtime": 1.234
  },
  "subsystems": [
    {"name": "function_call", "total_ms": 0.500}
  ],
  "opcodes": [
    {"code": 6, "name": "CODE_RTIVKFUNC", "count": 10}
  ],
  "functions": [
    {"name": "kernel", "calls": 1, "total_ms": 0.700, "self_ms": 0.300}
  ]
}
```

That is intentionally smaller than the original aspirational schema.

## Known Limitations of the Current Implementation

## Overlap Rules

Subsystem timing is currently overlapping and inclusive in practice.

That means:

- subsystem totals should be used for ranking
- subsystem totals should not be treated as a partition of total runtime
- large parent/child call stacks can make summed subsystem totals exceed total runtime

This is visible in real Track A data and should be documented explicitly in both the profiler docs and any future summarizer output.

## Missing Aggregate Counters

The current profiler does not yet record:

- bytes allocated
- property/dict/array/string operation counts
- native call counts beyond coarse subsystem timing
- runtime error counts

These are still important gaps for `binary-trees`, `fasta`, and container-heavy regressions.

## Missing Timing Detail

The current profiler does not yet record:

- per-opcode nanoseconds
- grouped opcode-family timings
- subsystem-exclusive timings
- quickening-installation time
- generic-fallback execution time

That limits how precisely a slowdown can be localized once a hot subsystem is identified.

## Missing Attribution

The current profiler does not yet record:

- bytecode offsets
- source file or line
- sample-based hotspot snapshots
- per-function allocation or opcode-family summaries

The current function view is still useful, but it is not yet enough for line-accurate diagnosis.

## Missing Benchmark and Report Tooling

The repository now has a dedicated runtime-profile comparison tool:

- [`tools/driver/profile/compare_runtime_profiles.py`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/profile/compare_runtime_profiles.py)

The remaining gap is higher-level integration that emits benchmark-oriented reports with:

- subsystem deltas
- opcode deltas
- function delta tables
- fallback and specialization changes

That remains a high-value next step.

## Design Goals That Still Stand

The original design goals still hold:

1. reuse the current driver/profile plumbing
2. keep profiling off by default and cheap when disabled
3. separate low-overhead aggregate profiling from deeper hotspot attribution
4. keep output machine-readable first and human-readable second
5. preserve benchmark comparability
6. avoid building a profiler that only explains one benchmark

## Updated Plan

## Phase 0: Plumbing and Schema

Status: mostly complete

Implemented:

- runtime profile data structure
- runtime profile JSON writer
- runtime profile summary printer
- CLI flags for runtime summary and JSON output
- interpreter profiling enable/disable plumbing

Remaining in this phase:

- document the implemented schema as the stable baseline
- add explicit overlap semantics to user-facing docs
- decide whether the current field names are final before expanding the schema again

## Phase 1: Coarse Runtime Timers

Status: substantially complete

Implemented:

- total runtime timing
- dispatch count
- function call count
- per-opcode execution counts
- coarse subsystem timings
- per-function inclusive/self timing

Remaining in this phase:

- add native-call count as an explicit counter, not only a timed subsystem
- decide whether to add dispatch-loop time separate from total runtime
- make summary output optionally include top opcodes

## Phase 2: Object and Container Profiling

Status: partially started

Already present:

- allocation count by object kind
- deallocation count by object kind
- refcount increment/decrement counts

Still needed:

- bytes allocated where practical
- dict/array/string operation counts
- container growth and storage churn signals

Primary hook points remain:

- [`src/runtime/RTObject.c`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTObject.c)
- [`src/runtime/RTEngine.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp)

## Phase 3: Timing Precision and Specialization Context

Status: partially started

Already present:

- function self time
- quickening counters

Still needed:

- per-opcode total nanoseconds
- grouped opcode-family totals
- quickening attempts by opcode family
- fallback reasons
- quickening/deopt time accounting

## Phase 4: Sampling Hotspots

Status: not started

Still needed:

- opt-in sampled interpreter-state profiling
- sample interval control
- capture of current function/opcode/subsystem/source span
- sampling-aware report output

This remains optional and should stay off by default.

## Phase 5: Benchmark Correlation and Comparison

Status: partially complete

Already present:

- Track A runtime-profile capture mode
- raw runtime-profile JSON next to benchmark output
- comparison script for two runtime profiles

Still needed:

- delta report by subsystem/opcode/function
- benchmark summary integration for runtime-profile findings

## Recommended Next Slice

The highest-value next slice is no longer "add runtime profiling". That already exists, and the immediate comparison/allocation slice is now in place.

The highest-value next slice is:

1. add benchmark-integrated runtime-profile delta reporting
2. add bytes-allocated and container-operation counters
3. add per-opcode timing only after the aggregate counters prove insufficient
4. defer sampling until aggregate comparison stops being enough

Reason:

- the current profiler already answers the first-order "where is time going?" question
- the next diagnostic gap is deeper object/container visibility and benchmark-facing delta output
- sampled hotspots can wait until aggregate counters stop being enough

## Validation Plan

Validation should continue to cover both correctness and profiler quality.

## Correctness checks

- profiling enabled must not change program output
- profiling enabled must not suppress runtime diagnostics
- compile profile behavior must remain intact
- Track A harness capture must still succeed when `--starbytes-runtime-profiles` is enabled

## Profiler sanity checks

- total runtime in runtime profile should approximately match `runtime_exec` from compile profile
- summed subsystem time must be documented as overlapping where applicable
- summed function inclusive time may exceed total runtime only in expected parent/child ways
- runtime profile JSON must remain readable by existing scripts and reports

## Overhead checks

Measure profiler overhead on at least:

- `binary-trees`
- `spectral-norm`
- `n-body`
- `fasta`

The intent remains:

- aggregate mode should be cheap enough for routine regression diagnosis
- deeper modes can be more expensive if they are clearly opt-in

## Expected Value

With the current implementation plus the remaining planned slices, Starbytes should be able to answer questions like:

- did `binary-trees` slow down because function execution, member access, or allocation churn changed?
- did `spectral-norm` regress because call-heavy helpers got slower or because typed numeric specialization stopped helping?
- did `k-nucleotide` move time into member-heavy container operations?
- did a new runtime change alter opcode mix, function mix, or specialization stability?

That is the level of visibility needed before making another broad runtime optimization pass.
