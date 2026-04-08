# Hyper-Efficient Execution Model Plan

## Purpose

Design a Starbytes execution model that is materially faster than the current runtime and can honestly be called a real JIT architecture once the full plan lands.

This plan is grounded in:

- the current Starbytes runtime/compiler shape
- Starbytes benchmark and runtime-profile data captured on March 25, 2026
- CPython's specializing interpreter and emerging Tier 2/JIT pipeline
- HotSpot's tiered profiling, frame model, and JIT/code-cache architecture

The recommendation is not "copy Python" or "copy Java".

The recommendation is:

- take CPython's very cheap, local adaptive specialization
- take HotSpot's explicit frame/profile/deopt discipline
- rebuild Starbytes around a register/slot VM with a feedback vector and a real hot-path compiler tier

## Key Assumptions

- Starbytes language semantics should stay stable while the runtime changes underneath them.
- a bytecode format/version bump is acceptable if the migration path is explicit
- x86_64 and arm64 are the only realistic first-wave JIT targets
- Track A is not the whole product, but it is a reliable enough hot-path signal to drive runtime architecture
- Tier 1 wins must be worthwhile even if Tier 2 takes longer than expected

## Current Diagnosis

Starbytes is no longer a purely naive boxed interpreter, but it is still structurally expensive in ways that cap performance.

### What already exists

- direct-call bytecode and decoded function bodies
- slot locals with typed numeric lanes
- typed numeric arrays
- specialized numeric bytecode
- narrow adaptive quickening for some numeric and index expressions
- runtime profiling by opcode/subsystem/function

Relevant current code paths:

- `src/runtime/RTEngine.cpp`
- `include/starbytes/compiler/RTCode.h`
- `src/compiler/CodeGen.cpp`

### What is still fundamentally expensive

1. Expression execution is still recursive and stream-driven.
   - `evalExpr(std::istream &)` recursively decodes nested expression trees from a byte stream.
   - The runtime does not execute from a contiguous instruction array with an explicit `pc`.

2. Function execution still enters through a stream abstraction.
   - decoded bodies are better than seek/restore on the shared module stream, but they still execute through `MemoryInputStream` and stream decoding rather than a tight instruction loop over decoded instructions.

3. There is no full feedback-vector model.
   - quickening exists for a narrow expression subset
   - there is still no site-local feedback for general calls, member dispatch, globals, branches, or layout guards

4. Broad opcodes still hide too much cost.
   - generic member invoke/get/set
   - generic index and collection behavior outside the typed slices
   - object-heavy paths still pay broad runtime lookup and allocation machinery

5. The runtime has no real JIT tier.
   - current quickening is interpreter specialization, not machine-code generation
   - there is no OSR, deopt metadata, code cache, or hot-loop compiler tier

## Current Runtime Signal

The March 25, 2026 steady-state benchmark/report and runtime profiles are enough to show that Starbytes does not have one bottleneck. It has three different execution-path bottlenecks:

- call/frame overhead
- member/layout dispatch
- index/numeric loop throughput

### Benchmark baseline

Source:

- `benchmark/results/summaries/track_a_steady-state_20260325T230531Z.report.md`

Selected results:

| workload | Starbytes | Python | relative to Python |
| --- | ---: | ---: | ---: |
| `binary-trees` | 28.103834s | 0.413248s | 68.0073x |
| `fasta` | 0.380569s | 0.044426s | 8.5664x |
| `k-nucleotide` | 0.609041s | 0.045660s | 13.3387x |
| `n-body` | 1.430978s | 0.048120s | 29.7374x |
| `spectral-norm` | 16.176500s | 0.103393s | 156.4558x |

Starbytes relative runtime geometric mean: `16.4598x` Python.

### Profile interpretation

#### `binary-trees`

Source:

- `benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_binary-trees.runtime-profile.json`

Signal:

- `70,170,564` dispatches
- `5,297,531` function calls
- `1,324,382` custom-class allocations
- `member_access` and `object_allocation` remain dominant inclusive subsystems

Interpretation:

- Starbytes still needs object/layout specialization and cheaper allocation
- direct calls helped, but field/object mechanics are still too generic

#### `k-nucleotide`

Source:

- `benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_k-nucleotide.runtime-profile.json`

Signal:

- `15,125` `CODE_RTMEMBER_IVK`
- `9,320` `CODE_RTMEMBER_GET`
- `member_access = 357.839 ms` out of `584.653 ms`

Interpretation:

- member/builtin dispatch is still the clear hot-path problem here

#### `n-body`

Source:

- `benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_n-body.runtime-profile.json`

Signal:

- `85,260` `CODE_RTTYPED_INDEX_GET`
- `37,503` `CODE_RTTYPED_INDEX_SET`
- `154,859` `CODE_RTTYPED_BINARY`
- `index_access = 1031.448 ms` out of `1498.169 ms`

Interpretation:

- Starbytes already has typed numeric machinery
- the remaining issue is throughput of typed indexed loops, not just missing numeric types

#### `spectral-norm`

Source:

- `benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_spectral-norm.runtime-profile.json`

Signal:

- `16,671,456` dispatches
- `460,804` direct calls
- `9,702,592` number allocations
- `460,797` quickened executions at exactly one site

Interpretation:

- current quickening can help a narrow hot pattern
- it is nowhere near broad enough to carry the whole runtime

## External Research

## 1. CPython lessons worth copying

### A. Specialize individual hot instructions cheaply

PEP 659 describes CPython's specializing adaptive interpreter.

The important design points are:

- instructions become adaptive at runtime
- specialization is local and cheap
- specialized instructions can quickly deopt back to generic behavior
- inline cache data lives adjacent to the instruction family that uses it

That matters for Starbytes because the current runtime still treats many high-frequency operations as broad generic cases.

Starbytes should copy the principle, not the exact opcode set:

- specialize small hot sites first
- keep fallback cheap
- let runtime feedback mutate the in-memory execution image

### B. Use raw argument spans instead of packaging calls

PEP 590 and the CPython call protocol document show why vectorcall matters:

- calls that already have arguments in contiguous memory should not allocate temporary tuples/dicts
- the runtime should pass a raw argument vector and optional keyword-name metadata

Starbytes has started moving in this direction with `DirectArgBuffer`, but it has not made this the universal fast-call ABI.

### C. Separate specialized bytecode from internal hot-path IR

Python 3.13 was released on October 7, 2024. Its experimental JIT path uses:

- specialized Tier 1 bytecode
- translation to an internal Tier 2 micro-op IR
- optimization over that IR
- copy-and-patch machine code generation

The key Starbytes lesson is:

- the bytecode format used for portability/debugging does not have to be the same format used for hot execution

## 2. HotSpot lessons worth copying

### A. Make the runtime an explicit frame/pc machine

The JVM spec defines:

- a per-thread `pc`
- frames
- compile-time-known frame sizes
- explicit local-variable and operand-stack storage

The important lesson is not "be stack-based like the JVM".

The important lesson is:

- the runtime should execute on explicit frames with known layout
- code should not be decoded recursively from a stream on the hot path

### B. Collect profile data per bytecode site

HotSpot's `MethodData` design is the most relevant external precedent for Starbytes.

The key ideas:

- each method has profile state
- profile points are aligned with bytecode sites
- counts and receiver/type information are stored per site
- tiered compilers use those profiles later

Starbytes currently has aggregate profiling, but not per-site profile ownership for general dispatch and optimization.

### C. Separate profiled code from optimized code

Tiered compilation and segmented code cache are important because they separate:

- warm/profiling-oriented code
- long-lived optimized code
- non-method VM code

Starbytes does not need HotSpot's full code-cache machinery immediately, but it should copy the architecture:

- warm code gathers feedback
- hotter code gets compiled
- optimized code has its own residency policy

### D. Deoptimization is part of the architecture, not an afterthought

HotSpot deoptimizes when assumptions fail.

That matters because any real Starbytes JIT will need:

- guards
- invalidation
- deopt maps
- a known way to rebuild interpreter state from compiled state

Without that, a "JIT" would just be brittle code generation glued onto the current interpreter.

## Recommended Target

## Starbytes should adopt a Tiered Register VM with Feedback Vectors and a Real Hot-Path JIT

This is the recommended execution model:

### Tier 0: Portable bytecode image

- compact serialized module format on disk
- stable across runs
- rich enough to carry frame layout, constant pools, call/member metadata, and deopt metadata

### Tier 1: Register/slot interpreter

- decoded once into a contiguous execution image
- real `pc`-driven frame machine
- direct-threaded or equivalent low-overhead dispatch
- split typed lanes for refs, integers, and doubles

### Tier 1.5: Adaptive specialization

- per-site feedback vector
- monomorphic inline caches first
- small PICs only after monomorphic caches are stable
- superinstructions created from hot instruction sequences

### Tier 2: Real JIT

- hot loops/functions lower into an internal micro-op IR
- IR is optimized
- IR becomes machine code
- OSR and deopt are built in from the start

This is a real JIT architecture.

It is also a realistic one for Starbytes because it does not require jumping directly from today's stream interpreter to a large LLVM-based optimizer.

## Bytecode Redesign

## Recommendation: move to Bytecode V2

The current bytecode stream is too expression-tree-oriented for a high-throughput VM.

Bytecode V2 should be:

- linear
- frame-oriented
- register/slot addressed
- compact on disk
- easy to decode into a fixed-layout in-memory execution image

### What should change

1. Replace recursive expression encoding with linear instruction streams.
   - current shape: nested expression decoding through `evalExpr(std::istream &)`
   - target shape: `pc` walks a flat instruction array

2. Make frame layout explicit per function.
   - ref-slot count
   - integer-slot count
   - double-slot count
   - temp/register count
   - feedback-slot count
   - exception/deopt metadata count

3. Encode semantic side tables explicitly.
   - constant pool
   - direct-call target table
   - builtin-member table
   - field-slot table
   - method-cache descriptors
   - branch/deopt map

4. Keep runtime specialization out of the serialized bytecode.
   - serialized bytecode should stay stable and portable
   - adaptive state belongs in the in-memory execution image and optional persisted profiles

### Recommended instruction families

The exact enum layout can vary, but the VM should support these families explicitly:

- moves/loads/stores
- typed arithmetic and comparisons
- typed array/string/dict access
- direct call / native call / builtin call
- member get/set/call with cache-index operands
- guard operations
- jumps and loop headers
- deopt/safepoint markers

Examples of instructions Starbytes should eventually have in the execution image:

- `LOAD_CONST`
- `MOVE`
- `LOAD_REF_SLOT`
- `STORE_REF_SLOT`
- `LOAD_I64_SLOT`
- `STORE_I64_SLOT`
- `LOAD_F64_SLOT`
- `STORE_F64_SLOT`
- `ADD_I64`
- `ADD_F64`
- `MUL_F64`
- `ARRAY_GET_F64`
- `ARRAY_SET_F64`
- `GET_FIELD_SLOT`
- `SET_FIELD_SLOT`
- `GET_BUILTIN_META`
- `CALL_DIRECT`
- `CALL_MONO`
- `CALL_NATIVE`
- `JUMP`
- `JUMP_IF_FALSE`
- `LOOP_HEADER`
- `GUARD_LAYOUT`
- `GUARD_TYPE`
- `DEOPT`

## In-Memory Execution Image

The runtime should not execute directly from the serialized byte stream.

It should load each function into something conceptually like:

```text
ExecFunction {
  Instr* code;
  uint32_t codeLen;
  uint16_t refSlots;
  uint16_t i64Slots;
  uint16_t f64Slots;
  uint16_t tempSlots;
  uint16_t feedbackSlots;
  Constant* consts;
  FeedbackDesc* feedbackDesc;
  DeoptMap* deoptMaps;
}
```

And each active call frame should look conceptually like:

```text
ExecFrame {
  ExecFunction* func;
  uint32_t pc;
  ExecFrame* caller;
  ValueRef* refs;
  int64_t* i64s;
  double* f64s;
  Tag* tags;
}
```

The exact structure can differ, but these properties matter:

- no stream decoding on the hot path
- no scope-string construction on function entry
- no dynamic local-slot resizing in steady state
- all hot metadata reachable by index, not by string lookup

## Feedback Vector Design

This is the architectural center of the new runtime.

Each function should own a feedback vector, with entries for sites such as:

- direct/generic call
- member get
- member set
- member call
- global/module lookup
- array access
- branch
- allocation site

Each feedback entry should carry only the data its site family needs.

### Examples

#### Call site

- execution count
- last target id
- exact-arity success count
- inline target pointer
- deopt counter

#### Member call site

- receiver layout version
- cached method target
- monomorphic miss count
- optional two-entry PIC data later

#### Field access site

- receiver layout version
- field slot
- guard failure count

#### Typed index site

- collection kind
- element kind
- bounds-check stability data

This is the Starbytes equivalent of combining:

- CPython's inline caches
- HotSpot's MethodData profile points

## Adaptive Specialization Plan

Starbytes should expand from today's narrow expression quickening into broad site-local specialization.

### First wave

- direct-call specialization from dynamic call sites
- monomorphic user-method caches
- builtin metadata fast paths such as `.length`
- field-slot fast paths everywhere layout proof exists
- global/module lookup caching
- typed array/string index specialization

### Second wave

- two-entry PICs for sites that are stably bimorphic
- small superinstructions for hot basic-block fragments
- bounds-check hoisting within proven loops
- loop-header hotness counters that trigger Tier 2 lowering

### Deoptimization behavior

Any specialized site must be able to:

- fall back immediately
- record the miss
- either stay specialized, widen to a broader cache, or deopt to generic form

The fallback path must stay cheap and correct.

## JIT Plan

## Recommendation: build a real but narrow JIT first

Do not begin with a full optimizing compiler.

Begin with a hot-loop and hot-leaf-function JIT that compiles from specialized bytecode into an internal micro-op IR, then into machine code using a low-maintenance strategy.

### Why this is the right first JIT

1. Starbytes does not yet have the frame/deopt/profile substrate for a large optimizing JIT.
2. The benchmark data shows hot loops and monomorphic helpers before it shows deeply polymorphic OO workloads.
3. A smaller JIT is more likely to land and stay correct.

### Tier 2 IR recommendation

Use a simple SSA-lite or registerized micro-op IR with:

- explicit loads/stores
- explicit typed arithmetic
- explicit guards
- explicit calls
- explicit deopt exits

The IR should be internal only.

This follows the Python 3.13 direction:

- Tier 1 specialized bytecode
- internal uop IR
- optimize IR
- emit code

### Code generation recommendation

Use copy-and-patch or template-based machine code generation first.

Why:

- lower maintenance than introducing a full LLVM/JIT dependency into runtime
- faster compile latency for hot loops
- more realistic for a small runtime team
- works well with a generated micro-op inventory

### What Tier 2 should optimize first

- hot counted loops
- typed numeric kernels
- monomorphic direct calls
- field-slot access with stable layout guards
- typed array loops

### What Tier 2 should not try to do first

- whole-program optimization
- large polymorphic inlining
- heroic alias analysis
- speculative optimization without deopt support

## Companion Runtime Changes

These are not optional if the goal is a hyper-efficient runtime.

### 1. Layout/version metadata

To make inline caches safe, Starbytes needs:

- class layout version ids
- method table version ids
- invalidation hooks when class metadata changes

### 2. Cheaper object layout

`binary-trees` shows that object-heavy code still pays too much for:

- allocation
- field storage
- field lookup

The object model should continue moving toward:

- fixed field slots for classes by default
- generic property bags as fallback, not default

### 3. Allocation strategy improvement

The current reference-counted heap model is a ceiling on object-heavy workloads.

A full GC redesign is a separate project, but the execution-model plan should assume at least one of:

- a nursery for short-lived objects
- stack/non-escaping temporary elision in the JIT
- refcount batching or deferred decrements on hot paths

Without this, the JIT will still spend too much time materializing temporary objects.

### 4. String and slice views

`fasta` still pays too much for tiny string creation.

The runtime should add:

- direct scalar/index paths
- substring/slice views where semantics allow
- string metadata fast paths without property-object churn

## Thin-Slice Rollout

This should not be attempted as a single rewrite.

Note:

- the V1 stream interpreter is a migration aid only
- once V2 reaches full semantic coverage and the compatibility window closes, the old interpreter should be removed entirely rather than kept as a permanent second execution path

## Phase 0: instrumentation and compatibility

- keep current profiler
- add per-site counters for call/member/index/branch candidates
- add bytecode versioning for V2
- preserve old bytecode execution during migration

Exit criteria:

- old and new runtimes can coexist behind a flag
- runtime profiles can distinguish V1 and V2 code paths

## Phase 1: Bytecode V2 plus new interpreter skeleton

- emit V2 for a restricted subset:
  - direct calls
  - typed numeric locals
  - branches
  - typed array ops
- load into `ExecFunction` arrays
- execute with explicit `pc`/frame machine

Thin slice target:

- `spectral_norm.A`
- the hot counted loops in `n-body`

Exit criteria:

- no `std::istream` on the hot path for V2 functions
- direct calls do not build scope strings
- correctness parity on the selected kernels

## Phase 2: feedback vectors and monomorphic caches

- per-function feedback vectors
- member call caches
- field-slot caches
- builtin metadata fast paths
- global lookup caches

Thin slice target:

- `k-nucleotide`
- `binary-trees`

Exit criteria:

- large drop in `CODE_RTMEMBER_IVK`-style generic fallback usage
- measurable reduction in `member_access` time

Note:

- some static phase-2-adjacent pieces already landed outside this plan:
  - builtin member call opcodes
  - direct field-slot get/set opcodes
- the current implementation pass intentionally narrows phase 2 to a runtime-owned thin slice:
  - per-site feedback state for the remaining generic paths
  - monomorphic caches for generic member get/invoke
  - scoped/global lookup caches
- broader Bytecode V2 lowering for member-heavy code should come after this runtime slice is validated

## Phase 3: superinstructions and loop formation

- fuse common instruction sequences in the execution image
- add loop-header hotness and guard stability tracking
- create Tier 2 IR from hot loops

Exit criteria:

- dispatch count falls materially for `n-body` and `spectral-norm`
- hot loops lower cleanly into the IR without semantic drift

## Phase 4: baseline JIT

- compile hot loops/functions from Tier 2 IR to machine code
- add OSR entry at loop headers
- add deopt exits back to Tier 1
- implement a simple segmented code cache:
  - warm/profiled code
  - optimized code
  - non-method VM code

Exit criteria:

- compiled hot loops are stable under deopt/fallback tests
- meaningful runtime wins on `spectral-norm` and `n-body`

## Phase 5: object/allocation integration

- scalar replacement or non-escaping temporary elimination
- nursery or other young-object strategy
- stronger fixed-layout object support

Exit criteria:

- `binary-trees` stops being dominated by generic object costs

## Success Metrics

The plan should be judged by these concrete outcomes, not by whether it sounds advanced.

### Architectural metrics

- V2 functions execute without `std::istream` decoding on the hot path
- all hot functions use explicit frames and `pc`
- hot dynamic sites have feedback-vector ownership
- compiled code can deopt back to the interpreter

### Profile metrics

- large reductions in generic member opcodes for `k-nucleotide` and `binary-trees`
- large reductions in dispatch count for `spectral-norm` and `n-body`
- lower number allocation counts in typed numeric kernels
- lower refcount churn on hot object-heavy paths

### Benchmark metrics

Short term:

- cut `spectral-norm` and `n-body` materially before touching more exotic workloads
- move `k-nucleotide` by attacking member dispatch directly

Medium term:

- push the Track A steady-state geometric mean well below the current `16.4598x` Python baseline

## Decisions And Tradeoffs

### Decision 1: register/slot VM vs stack VM

Recommendation:

- use a register/slot VM

Reasoning:

- Starbytes already has slot locals and typed slots
- register code lowers more directly into caches and JIT IR
- fewer dispatches than a stack VM for many current kernels

Tradeoff:

- bytecode gets somewhat larger unless compacted carefully

### Decision 2: serialize quickened opcodes or not

Recommendation:

- do not serialize specialization state into module files

Reasoning:

- keeps bytecode portable and deterministic
- avoids profile poisoning across environments
- mirrors the CPython split between stable bytecode and mutable execution state

### Decision 3: LLVM-first JIT vs generated/template JIT

Recommendation:

- do generated/template copy-and-patch first

Reasoning:

- lower engineering and maintenance cost
- better fit for a runtime that still needs to build its frame/deopt substrate
- aligns with current Starbytes scale better than a large compiler framework integration

### Decision 4: monomorphic caches before PICs

Recommendation:

- yes

Reasoning:

- simpler invalidation model
- easier to debug
- likely enough for a large share of current Starbytes hot sites

## What Not To Do

1. Do not bolt a machine-code emitter onto the current recursive stream interpreter.
2. Do not keep expanding broad generic opcodes when the profile already identifies stable call/member/index shapes.
3. Do not serialize runtime quickening state into `.stbxm` files.
4. Do not build polymorphic caching before layout/version invalidation exists.
5. Do not wait for a perfect optimizing JIT before fixing the interpreter architecture.

## Recommendation Summary

The recommended Starbytes execution model is:

1. `Bytecode V2`: a linear register/slot bytecode with explicit function metadata.
2. `Execution image`: decode once into contiguous instruction arrays.
3. `Tier 1 interpreter`: explicit frame/pc machine with typed slot lanes.
4. `Feedback vector`: per-site counters and cache state for calls, members, indexes, branches, and allocation sites.
5. `Adaptive specialization`: monomorphic caches, typed fast paths, and superinstructions.
6. `Tier 2 JIT`: internal micro-op IR plus copy-and-patch machine code, with OSR and deopt.
7. `Companion runtime work`: layout versioning, fixed-slot objects, and cheaper short-lived allocation.

That is the smallest architecture that can realistically deliver:

- major interpreter wins immediately
- a credible real JIT later
- a path that matches Starbytes' actual benchmark bottlenecks instead of chasing a generic VM fantasy

## References

Python:

- PEP 659: <https://peps.python.org/pep-0659/>
- PEP 590: <https://peps.python.org/pep-0590/>
- Python C API call protocol: <https://docs.python.org/3/c-api/call.html>
- What’s New in Python 3.13: <https://docs.python.org/3/whatsnew/3.13.html>
- PEP 744: <https://peps.python.org/pep-0744/>

Java / HotSpot:

- JVM Spec, Chapter 2: <https://docs.oracle.com/javase/specs/jvms/se22/html/jvms-2.html>
- HotSpot MethodData overview: <https://wiki.openjdk.org/display/HotSpot/MethodData>
- HotSpot performance techniques and deoptimization: <https://wiki.openjdk.org/spaces/HotSpot/pages/11829300/PerformanceTechniques>
- JEP 197 Segmented Code Cache: <https://openjdk.org/jeps/197>
- Oracle Java HotSpot performance enhancements: <https://docs.oracle.com/en/java/javase/24/vm/java-hotspot-virtual-machine-performance-enhancements.html>

Starbytes internal grounding:

- `src/runtime/RTEngine.cpp`
- `include/starbytes/compiler/RTCode.h`
- `docs/NumericalExecutionImprovementPlan.md`
- `docs/FunctionExecutionHandlingProposal.md`
- `docs/MemberBuiltinDispatchPlan.md`
- `docs/ObjectAllocationOptimizationPlan.md`
