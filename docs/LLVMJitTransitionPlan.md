# LLVM JIT Transition Plan

## Purpose

Define how Starbytes should transition from the baseline internal JIT proposed in `docs/HyperEfficientExecutionModelPlan.md` to an LLVM-backed JIT tier after the current JIT architecture has been implemented, tested, and proven stable.

This is a follow-on plan, not a replacement for the current execution-model plan.

The first job is still:

- land Bytecode V2
- land the explicit frame/`pc` interpreter
- land feedback vectors and site-local caches
- land a real baseline JIT with OSR and deopt
- prove that the runtime substrate is correct under stress

Only after that is stable should Starbytes consider paying the cost of LLVM integration.

## Relationship To The Current Plan

The current execution-model plan explicitly recommends:

- a register/slot VM
- mutable in-memory execution images
- per-site feedback vectors
- a narrow internal Tier 2 IR
- copy-and-patch or template-based code generation first

This document does **not** reverse that recommendation.

It assumes that recommendation was correct for the first implementation wave and asks a later question:

- once the baseline JIT exists and is stable, when should Starbytes move part or all of compiled code generation onto LLVM?

## Activation Gate

This plan should stay dormant until all of the following are true:

1. The Phase 4 exit criteria in `docs/HyperEfficientExecutionModelPlan.md` are met.
   - compiled hot loops/functions are already stable under deopt/fallback tests
   - `spectral-norm` and `n-body` already show meaningful runtime wins

2. The runtime substrate has stopped changing in large ways.
   - frame layout is stable
   - deopt state reconstruction is stable
   - OSR entry/exit semantics are stable
   - feedback-vector ownership is stable

3. There is evidence that the baseline JIT is becoming the limiting factor.
   - compile latency is acceptable, but generated code quality is leaving performance on the table
   - or backend maintenance across x86_64 and arm64 is growing costly
   - or future optimization work is being blocked by backend complexity rather than runtime semantics

4. The build and release process can absorb a large optional toolchain dependency.
   - local builds remain workable
   - CI can support both LLVM-enabled and non-LLVM configurations

If these conditions are not met, Starbytes should keep investing in the current baseline JIT architecture instead of starting an LLVM migration early.

## Why LLVM Becomes Reasonable Later

LLVM is attractive only after Starbytes has already built the runtime discipline that LLVM cannot provide by itself.

LLVM can help with:

- target code generation quality
- register allocation
- instruction selection
- mature optimization passes
- keeping x86_64 and arm64 backends from diverging too much

LLVM does **not** solve:

- feedback-vector design
- inline-cache discipline
- guard placement
- deopt semantics
- OSR semantics
- runtime invalidation/versioning
- object-model cost
- allocation-model cost

That is why LLVM should come after the baseline JIT, not before it.

## Recommended Target

## Move To A Two-Compiler Tiered JIT, Not An LLVM-First Rewrite

The safest target state is:

- Tier 1: register/slot interpreter with feedback vectors
- Tier 2: existing low-latency baseline JIT for warm code
- Tier 3: LLVM-backed optimizing JIT for hotter and more stable code

This preserves the main advantages of the current plan:

- cheap warm-up
- fast compile latency for hot loops
- runtime-owned guards/deopt discipline
- a fallback path that does not depend on LLVM

And it lets LLVM do the work it is best at:

- deeper target-specific code generation for the hottest regions

Starbytes should treat LLVM as an optimizing backend layered on top of the internal JIT architecture, not as the new source of truth for runtime semantics.

## Core Architectural Rule

LLVM should consume Starbytes' internal JIT IR after profiling, specialization, guard insertion, and deopt planning have already happened.

That means:

- serialized bytecode stays unchanged in principle
- the execution image stays Starbytes-owned
- feedback vectors stay Starbytes-owned
- deopt maps stay Starbytes-owned
- guard and safepoint semantics stay Starbytes-owned
- LLVM IR is a lowering target, not the canonical runtime IR

This boundary is important because it keeps the runtime architecture coherent even if LLVM is later disabled, upgraded, or replaced.

## Prerequisites

Before starting the migration, Starbytes should have all of the following:

### 1. Stable internal JIT IR

The current micro-op IR must already encode:

- typed loads/stores
- typed arithmetic
- explicit calls
- explicit guards
- explicit deopt exits
- safepoints
- loop-header metadata

If those are still in flux, LLVM integration will just freeze unstable decisions into a larger system.

### 2. Stable compiled-frame ABI

The runtime needs a well-defined ABI for:

- interpreter to compiled entry
- OSR entry at loop headers
- compiled to interpreter deopt exit
- compiled calls into runtime helpers
- exception propagation and unwinding behavior

LLVM should target that ABI rather than inventing one ad hoc per backend.

### 3. Stable deopt and materialization rules

The runtime must already know how to rebuild interpreter state from compiled state, including:

- slot values
- temporary values that must be materialized
- live references
- `pc`/site re-entry
- guard failure re-entry

Without this, LLVM would only make failures harder to diagnose.

### 4. Stable invalidation/versioning

The runtime needs:

- layout version ids
- method version ids
- invalidation hooks
- compiled-code dependency tracking

Compiled LLVM code must be invalidated by the same versioning/invalidation system as the baseline JIT.

### 5. Benchmarks and stress tests that already matter

Before migration begins, Starbytes should already have:

- differential correctness tests between interpreter and baseline JIT
- deopt stress tests
- OSR stress tests
- benchmark automation for Track A kernels
- compile-latency and code-size measurements

LLVM should enter a runtime that can already prove correctness.

## Backend Recommendation

## Use LLVM ORC As A Backend, Not MCJIT-Style Legacy Plumbing

The preferred direction is:

- keep the Starbytes IR and tier manager
- lower selected hot regions into LLVM IR
- use LLVM's modern JIT/runtime object loading path
- keep executable-code ownership and metadata visible to Starbytes

Starbytes should avoid a design where LLVM becomes a hidden subsystem that owns too much of:

- code cache policy
- deopt metadata
- runtime symbol policy
- invalidation policy

The runtime should stay in charge of compiled-method lifecycle even when LLVM emits the machine code.

## Migration Phases

This should be a staged transition, not a rewrite.

## Phase 0: Go/No-Go Review

Before writing LLVM integration code, do a formal review against these questions:

1. Is the baseline JIT stable enough that backend changes will not be masked by runtime churn?
2. Are the current benchmark ceilings really backend/codegen ceilings rather than object-model or dispatch ceilings?
3. Is cross-target backend maintenance now expensive enough to justify LLVM?
4. Can CI and local builds tolerate an optional heavyweight dependency?

If the answers do not converge toward "yes", stop here.

## Phase 1: Isolate The Existing JIT Backend

Refactor the current baseline JIT into clean stages:

- hot-region selection
- IR building
- guard/deopt annotation
- backend lowering
- code installation
- code-cache bookkeeping

The important outcome is a backend interface that lets Starbytes keep one tier manager while swapping code generators underneath it.

Exit criteria:

- the current baseline backend works through the new interface
- LLVM can be added without rewriting hotness/deopt logic

## Phase 2: LLVM Spike On A Tiny Kernel Slice

Start with one very narrow slice:

- counted typed numeric loops
- no polymorphic inline caching
- no complex object materialization
- no exceptions beyond existing helper calls

Good initial targets:

- hot `spectral-norm` loops
- the typed numeric core of `n-body`

The goal is not big speedups yet.

The goal is to validate:

- lowering from Starbytes IR to LLVM IR
- machine-code installation
- runtime helper calls
- safepoints
- deopt exits back to Tier 1

Exit criteria:

- LLVM-compiled kernels are correct under stress
- deopt and fallback behavior match the baseline backend
- compile latency is measured and understood

## Phase 3: Production-Grade LLVM Backend For The Narrow Hot Path

Expand support to the same narrow slice the baseline JIT already handles best:

- hot counted loops
- monomorphic leaf helpers
- typed array/index kernels
- stable field-slot fast paths with explicit guards

Still defer:

- wide polymorphic inlining
- aggressive speculative object optimization
- complicated exception lowering
- broad whole-function compilation for unstable code

Exit criteria:

- LLVM code is semantically interchangeable with the baseline backend on the supported subset
- x86_64 support is solid
- arm64 support is validated before broader rollout

## Phase 4: OSR And Deopt Parity

LLVM integration is not real until it supports the same recovery model as the baseline JIT.

That means:

- OSR into LLVM-compiled loop bodies
- deopt from LLVM code back to Tier 1
- invalidation-triggered exits
- guard-failure exits
- correct live-reference handling at safepoints

Exit criteria:

- deopt parity suite passes for interpreter, baseline JIT, and LLVM JIT
- invalidation and fallback behavior are deterministic and diagnosable

## Phase 5: Selective Optimization Expansion

Once parity exists, add a small curated optimization pipeline.

Recommendation:

- start from a deliberately small pass set
- keep guards and deopt exits explicit
- add inlining only for monomorphic direct-call cases that already prove stable in profiles

Do **not** begin with a "turn on every aggressive optimization" mindset.

Starbytes should prefer a controlled pass inventory over a broad opaque optimization pipeline.

Exit criteria:

- LLVM materially beats the baseline backend on the hottest stable kernels
- compile latency remains acceptable for the tier policy

## Phase 6: Tier Policy And Residency

At this point the runtime should decide which compiler tier owns which temperature band.

Recommended split:

- baseline backend handles warm code and low-latency compilation
- LLVM backend handles only hotter, longer-lived, or repeatedly re-entered regions

The tier manager should use measured inputs such as:

- loop/function hotness
- guard stability
- historical deopt rate
- prior compilation cost
- code size and residency pressure

LLVM should not become the default compiler for every warm method.

## Phase 7: Re-Evaluate Full Replacement vs Permanent Hybrid

Only after the hybrid model is proven should Starbytes decide whether to:

- keep the baseline backend permanently as the low-latency compiler
- or move more compiled code onto LLVM

The default assumption should be:

- keep the hybrid model unless the data strongly favors consolidation

There is no need to remove a working baseline compiler just because LLVM exists.

## Build And Distribution Plan

LLVM integration should be optional first.

Recommended build policy:

- add a dedicated CMake option such as `STARBYTES_ENABLE_LLVM_JIT`
- default it to `OFF` in the first wave
- isolate LLVM headers and link dependencies to dedicated JIT targets
- keep non-JIT builds working
- keep parser/compiler-only work from depending on LLVM

CI policy should include:

- one configuration without LLVM
- one LLVM-enabled configuration for x86_64
- one LLVM-enabled configuration for arm64 when available

The important rule is:

- LLVM should not make the base contributor workflow substantially worse before it proves runtime value

## Testing Plan

LLVM transition work should be judged primarily by correctness and diagnosability.

Required test categories:

- interpreter vs baseline JIT vs LLVM JIT differential tests
- deopt re-entry tests
- OSR transition tests
- invalidation/version-change tests
- exception propagation tests
- GC/refcount/live-reference safety tests
- code-cache eviction and re-entry tests
- compile-latency budget tests

Profile and benchmark checks should include:

- `spectral-norm`
- `n-body`
- `k-nucleotide`
- `binary-trees`

The runtime should also maintain a mode that forces:

- interpreter only
- baseline JIT only
- LLVM JIT eligible

That makes failures diagnosable instead of blended together.

## Success Metrics

The LLVM transition is successful only if it achieves all of the following:

### Correctness

- no semantic drift relative to interpreter and baseline JIT
- stable deopt and OSR behavior under stress
- deterministic invalidation behavior

### Operability

- failures are diagnosable with runtime-owned metadata
- LLVM-generated code can be disabled without losing the rest of the runtime architecture
- code-cache and compiled-method ownership stay understandable

### Performance

- LLVM materially outperforms the baseline backend on the hottest stable kernels
- compile latency is low enough that tiering remains beneficial
- code size growth stays within the code-cache budget

### Maintenance

- x86_64 and arm64 backend work is cheaper overall than continuing to hand-maintain both paths
- the build/dependency burden remains acceptable for the team

## Go/No-Go Heuristics

Starbytes should **go forward** with LLVM if:

- the runtime substrate is already stable
- the baseline backend is now the main optimization ceiling
- LLVM shows real gains on long-lived hot regions
- toolchain cost is manageable

Starbytes should **not** go forward yet if:

- deopt or OSR is still fragile
- benchmark bottlenecks are still mostly member dispatch, object layout, or allocation
- LLVM only wins on microbenchmarks with unacceptable compile-latency cost
- the team cannot comfortably support the dependency and CI footprint

## What Not To Do

1. Do not start LLVM integration before baseline JIT deopt/fallback behavior is already proven.
2. Do not replace the Starbytes IR with LLVM IR as the canonical runtime representation.
3. Do not route all warm code through LLVM and lose low-latency compilation.
4. Do not let LLVM headers and APIs bleed through the general runtime interface surface.
5. Do not trust broad optimization pipelines before deopt, safepoint, and invalidation semantics are thoroughly tested.
6. Do not remove the baseline backend until the hybrid design has been measured over time.

## Recommendation Summary

The right LLVM transition for Starbytes is:

1. finish the current baseline JIT plan first
2. treat LLVM as a later optimizing backend, not the first JIT substrate
3. preserve the Starbytes-owned IR, guards, deopt maps, and tier manager
4. keep the baseline backend for low-latency warm compilation
5. use LLVM only for hotter, more stable regions once the runtime semantics are already trustworthy

That path gives Starbytes the upside of LLVM without paying its complexity cost before the runtime is ready for it.

## Internal Grounding

- `docs/HyperEfficientExecutionModelPlan.md`
- `docs/TypedJitVmCacheGapMatrix.md`
- `docs/NumericalExecutionImprovementPlan.md`
- `src/runtime/RTEngine.cpp`
- `include/starbytes/compiler/RTCode.h`
- `src/compiler/CodeGen.cpp`
- `CMakeLists.txt`
