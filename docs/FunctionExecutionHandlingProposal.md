# Function Execution Handling Proposal

## Goal

Design a new Starbytes function execution path that attacks the current Track A bottlenecks without requiring a JIT as the first move.

This proposal is driven by:

- the current Starbytes runtime and profile data
- CPython source code
- V8 source code

The target is a materially faster interpreter call path for:

- exact-arity Starbytes function calls
- monomorphic method calls
- builtin/native calls
- small hot helper functions in numeric kernels

## Current Signal

### Benchmark evidence

Measured from:

- `benchmark/results/raw/track_a_starbytes_steady-state_20260325T182048Z`
- baseline comparison: `benchmark/results/summaries/track_a_steady-state_20260309T014224Z.report.md`

Key results:

| workload | current mean(s) | prior mean(s) | slowdown |
|---|---:|---:|---:|
| binary-trees | 196.812 | 39.414 | 4.99x |
| spectral-norm | 41.044 | 6.431 | 6.38x |
| n-body | 1.396 | 1.026 | 1.36x |
| k-nucleotide | 0.606 | 0.127 | 4.76x |
| fasta | 0.520 | 0.170 | 3.07x |
| regex-redux | 0.031 | 0.023 | 1.33x |

The two dominant failures are:

- `binary-trees`: runtime profile shows `makeTree` and `checkTree` dominating, with very high function-call and member-access cost.
- `spectral-norm`: helper `A` is called `460,800` times and alone accounts for about `29.2s` self time in the runtime profile.

### What the current runtime is doing

The current hot path still routes many calls through generic object machinery:

- generic function invocation opcode dispatch: [`src/runtime/RTEngine.cpp:2289`](../src/runtime/RTEngine.cpp#L2289)
- generic function entry and argument materialization: [`src/runtime/RTEngine.cpp:2105`](../src/runtime/RTEngine.cpp#L2105)
- alternate value-based call path with the same frame/setup model: [`src/runtime/RTEngine.cpp:1894`](../src/runtime/RTEngine.cpp#L1894)
- member invocation gathers args dynamically and then performs runtime method lookup: [`src/runtime/RTEngine.cpp:3533`](../src/runtime/RTEngine.cpp#L3533)
- codegen currently emits only two broad call opcodes for most calls: [`src/compiler/CodeGen.cpp:1669`](../src/compiler/CodeGen.cpp#L1669), [`src/compiler/CodeGen.cpp:1721`](../src/compiler/CodeGen.cpp#L1721)

Observed costs in that path:

- per-call `std::vector<StarbytesObject>` argument collection
- per-call `Twine` scope-string construction
- per-call `pushLocalFrame()` resizing and object-slot ownership setup
- stream seeking into callee bytecode bodies
- generic `StarbytesFuncRef` indirection for direct function calls
- method dispatch by runtime string/class lookup
- builtin member dispatch through a large string-comparison ladder

This is exactly the wrong shape for:

- recursive exact-function calls like `binary-trees`
- tiny helper calls like `spectral-norm.A`
- repeated collection/string method calls like `k-nucleotide` and `fasta`

## External Grounding

### CPython

CPython uses a dedicated fast-call ABI instead of forcing all calls through tuple/dict packaging:

- `_PyObject_VectorcallTstate(...)` receives a raw argument array and only falls back to `tp_call` when no vectorcall entry exists:
  - [pycore_call.h#L839-L868](https://github.com/python/cpython/blob/main/Include/internal/pycore_call.h#L839-L868)

CPython also has specialized interpreter paths that push a new interpreter frame directly and continue dispatch in the callee instead of bouncing through a generic object-call slow path:

- `BINARY_OP_SUBSCR_GETITEM` caches a Python function target, pushes `_PyFrame_PushUnchecked(...)`, then `DISPATCH()` continues in the new frame:
  - [generated_cases.c.h#L3668-L3709](https://github.com/python/cpython/blob/main/Python/generated_cases.c.h#L3668-L3709)

The important CPython lesson is not "copy this opcode exactly". It is:

- keep a raw argument-span ABI
- let exact known callees enter frames directly
- keep the generic object-call protocol as fallback, not the default

### V8

V8 separates call shapes at bytecode generation time and assigns feedback slots to them:

- property calls, undefined-receiver calls, and generic calls are emitted differently, each with `AddCallICSlot()` feedback:
  - [bytecode-generator.cc#L6290-L6307](https://chromium.googlesource.com/v8/v8/+/master/src/interpreter/bytecode-generator.cc#6290)
  - [feedback-vector.h#L233-L282](https://chromium.googlesource.com/v8/v8/%2B/52ab610bd13/src/feedback-vector.h#233)

V8 also carries per-function invocation and optimization state in its feedback vector:

- feedback vector header includes invocation count, profiler ticks, and optimized code cell:
  - [feedback-vector.h#L105-L145](https://chromium.googlesource.com/v8/v8/%2B/52ab610bd13/src/feedback-vector.h#105)

V8's interpreter entry consults that feedback before normal frame setup:

- load feedback vector
- check tiering/optimized-code state
- increment invocation count
- then push the stack frame
  - [builtins-x64.cc#L887-L919](https://chromium.googlesource.com/v8/v8/%2B/fbb9df7218ae348b56104ff8fdd82b578e42d189/src/builtins/x64/builtins-x64.cc#887)
  - [builtins-x64.cc#L904-L906](https://chromium.googlesource.com/v8/v8/%2B/fbb9df7218ae348b56104ff8fdd82b578e42d189/src/builtins/x64/builtins-x64.cc#904)

The important V8 lesson is:

- classify call shapes early
- attach call-site feedback to them
- let feedback drive specialization of hot call sites

## Assumptions

- `binary-trees` and `spectral-norm` are the primary runtime priorities right now.
- We want an interpreter-first solution before a full optimizing JIT.
- Starbytes function/method identities are stable enough within a module load to support call-site caching.
- Preserving the full dynamic language surface still matters, so there must remain a generic fallback path.

## Proposal

## 1. Replace the current generic call path with a stack-span fast call ABI

Introduce one internal call entry point:

```cpp
StarbytesObject enterCall(const RTCallTarget &target,
                          StackArgSpan args,
                          ReceiverMode receiverMode,
                          StarbytesObject boundSelf,
                          CallSiteState *site);
```

Properties:

- `StackArgSpan` points at already-evaluated arguments in a contiguous VM-owned array.
- No per-call `std::vector<StarbytesObject>` allocation on the exact-function path.
- No `StarbytesFuncRef` object creation for statically resolved direct calls.
- No `Twine` scope-string creation on the hot path.
- One call entry for Starbytes, builtin, and native callees, with specialized subpaths.

This is the direct Starbytes analogue of CPython's vectorcall idea: raw args first, generic object calling only as fallback.

## 2. Stop using `std::istream` position switching as the primary callee-entry mechanism

Today the call path seeks into the callee bytecode body, runs until `CODE_RTFUNCBLOCK_END`, then restores the prior stream position. That is mechanically expensive and keeps calls tied to stream state.

Replace it with a decoded runtime code image:

- each function owns:
  - `bytecode_begin`
  - `bytecode_end`
  - local-slot layout
  - argument count
  - flags
- each active frame stores:
  - `pc`
  - `frame_base`
  - `operand_base`
  - `caller`
  - `return_pc`

This lets the interpreter:

- jump directly into a callee frame
- return without stream seek/restore
- support future inline-call and tail-call work

This follows the same broad direction as CPython's direct interpreter-frame handoff in generated cases.

## 3. Split one broad call opcode into explicit call shapes

Replace the current "mostly everything goes through `CODE_RTIVKFUNC` or `CODE_RTMEMBER_IVK`" design with these bytecodes:

- `CALL_DIRECT func_index argc`
- `CALL_METHOD_DIRECT class_id method_slot argc`
- `CALL_BUILTIN builtin_id argc`
- `CALL_NATIVE native_index argc`
- `CALL_CLOSURE slot argc`
- `CALL_DYNAMIC argc`

Rules:

- `CALL_DIRECT` is used when semantic/codegen can name the exact runtime function.
- `CALL_METHOD_DIRECT` is used when the receiver class slot is statically known.
- `CALL_BUILTIN` is used for stdlib/runtime builtins and hot container/string methods.
- `CALL_DYNAMIC` remains the fallback for true dynamic callable values.

This mirrors V8's choice to distinguish property-call and generic-call shapes at bytecode generation time instead of hoping one runtime path serves all of them well.

## 4. Add per-call-site feedback and quickening for calls

Create a `RuntimeCallSiteFeedback` table indexed by function bytecode offset.

Each entry should track:

- execution count
- last callee id
- last receiver class id
- observed arity
- monomorphic vs polymorphic state
- fast-path hit count
- deopt/fallback count

Use it to quicken:

- `CALL_DYNAMIC` -> `CALL_DIRECT` when the same function target repeats
- `CALL_DYNAMIC` -> `CALL_METHOD_DIRECT` when the same receiver class + method repeats
- `CALL_METHOD_DIRECT` -> polymorphic mini-cache when a small set of receiver classes appears

This is the Starbytes equivalent of V8's `AddCallICSlot()` plus feedback-vector driven specialization, but kept interpreter-only.

## 5. Introduce a reusable frame arena instead of per-call frame rebuilding

The current local frame setup resizes vectors and transfers ownership object-by-object. That is too expensive for `makeTree`, `checkTree`, and `A`.

Replace it with:

- a VM frame arena or free-list
- fixed layout from compile-time metadata
- inline storage for the common small-frame case
- typed slot storage separated from object slots

Target properties:

- no `std::vector::resize()` in the hot call path
- no repeated slot-kind initialization for every invocation
- no per-call allocator scope registration for locals

Frame lifecycle becomes:

1. reserve frame from arena
2. copy or move raw argument span into slot window
3. install `self` into a known receiver slot if present
4. jump to `pc = target.bytecode_begin`
5. release frame back to free-list on return

## 6. Move `self` and locals fully into frame slots, not scope maps

Right now the interpreter still interacts with allocator/scope machinery in function entry. That adds bookkeeping noise to exact-function calls.

For fast-call eligible functions:

- `self` lives in a dedicated receiver slot
- args live in arg slots
- locals live in local slots
- the scope map is only used when a function actually needs dynamic name semantics

Fast-call eligibility should require:

- no dynamic scope access
- no reflective locals lookup
- no runtime feature that depends on allocator-backed local name maps

This reduces overhead for the Track A kernels, which are structurally simple and slot-friendly.

## 7. Give member calls direct method slots and builtin ids

`CODE_RTMEMBER_IVK` is doing too much:

- generic receiver evaluation
- member-name string decoding
- collection/string/regex/dict builtin dispatch by string comparison
- dynamic class hierarchy lookup
- final call dispatch

That should become two layers:

### Builtin member fast path

At codegen or first quickening, map hot builtin members to compact ids:

- `ARRAY_PUSH`
- `ARRAY_AT`
- `STRING_UPPER`
- `STRING_SPLIT`
- `DICT_GET`
- etc.

Then dispatch by enum, not by repeated string compare.

### User method fast path

For monomorphic receivers:

- cache `receiver_class_id -> runtime_method*`
- validate class id
- enter direct method call

This is especially relevant to:

- `binary-trees`, where object/member traffic is large
- `k-nucleotide`, where member-access cost is dominant

## 8. Add an exact-arity leaf-function fast path

The profile says `spectral-norm.A` is the hot function, and it is tiny. Paying full generic call overhead for it is wasteful.

Add a leaf fast path for functions that are:

- exact arity
- non-lazy
- non-native
- no microtask interaction
- no dynamic scope requirements
- no variadic/keyword-like behavior
- small bytecode body

For those functions, allow:

- direct frame entry with no generic-call fallback checks inside the hot path
- optional inline expansion after the call site becomes hot and monomorphic

This is the highest-leverage fix for `spectral-norm`.

Important constraint:

- do not start with general-purpose inlining across the whole language
- start with exact-arity leaf-call quickening only

That keeps the first slice reversible.

## Proposed Runtime Shape

### Fast path

1. Evaluate callee and args into operand stack slots.
2. Load call-site feedback record.
3. If site is specialized and guards hold:
   - direct-enter Starbytes frame, builtin handler, or native callback
4. Else:
   - take generic resolver
   - update feedback
   - maybe quicken site

### Generic fallback

Fallback still supports:

- function refs
- dynamic callable values
- lazy functions
- reflective behavior
- any feature not yet expressible in a fast call opcode

The difference is that fallback is no longer the default.

## Why this tackles the measured problems

### binary-trees

Current profile:

- `makeTree`: `1,324,382` calls
- `checkTree`: `1,324,382` calls
- heavy `CODE_RTMEMBER_GET`, `CODE_RTMEMBER_SET`, local refs, object creation

Impact of proposal:

- exact direct recursive call path removes generic function-ref and arg-vector overhead
- reusable frames remove repeated frame allocation/setup cost
- receiver/local slots cut scope-map work
- monomorphic method/member fast path reduces member dispatch overhead

### spectral-norm

Current profile:

- `A`: `460,800` calls
- about `29.2s` self time

Impact of proposal:

- `CALL_DIRECT` exact-arity leaf fast path removes most current per-call tax
- call-site feedback can quicken this helper aggressively
- optional leaf inline expansion is feasible later if the frame fast path is still not enough

### n-body

Current profile is mostly typed index access, not function calls. This proposal helps less there, which is correct. It should not be sold as the whole runtime answer.

### k-nucleotide and fasta

These benefit mostly from:

- builtin member ids
- monomorphic member dispatch
- less generic call packaging for helper functions

### regex-redux

Runtime is tiny relative to total benchmark wall time, so function-call redesign is not the priority there.

## Non-goals

- No full JIT in this phase.
- No speculative deoptimization framework beyond call-site quickening guards.
- No attempt to solve all collection/index costs here.
- No attempt to make dynamic reflective calls as fast as exact direct calls.

## Thin-Slice Rollout

### Phase 1

Implement only:

- decoded code image
- frame arena
- `CALL_DIRECT`
- exact-arity direct Starbytes frame entry

Success criteria:

- `spectral-norm` improves materially
- `binary-trees` drops significantly
- no semantic change on existing runtime-profile tests

### Phase 2

Implement:

- call-site feedback table
- quickening from dynamic/direct and member/direct
- builtin member ids for hottest stdlib methods

Success criteria:

- `k-nucleotide` and `fasta` improve
- fallback counts become visible in runtime profiles

### Phase 3

Implement:

- exact-arity leaf-call fast path
- optional tiny-function inline quickening for proven hot monomorphic sites

Success criteria:

- `spectral-norm.A` no longer dominates runtime at its current level

## Validation

Track with:

- existing runtime profile JSON
- new call-site counters:
  - fast direct calls
  - generic fallback calls
  - monomorphic method hits
  - polymorphic method hits
  - call-site deopts

Add benchmark gates for:

- `binary-trees`
- `spectral-norm`
- `k-nucleotide`

And add correctness tests for:

- direct function calls
- bound method calls
- builtin member fast paths
- fallback on dynamic call target changes

## Recommendation

The first implementation slice should be:

1. decoded in-memory bytecode bodies
2. frame arena
3. `CALL_DIRECT`
4. exact-arity direct frame entry

That is the smallest end-to-end change that aligns with both:

- CPython's raw-argument plus direct-frame fast path
- V8's call-shape separation and feedback-driven specialization

It directly attacks the two worst measured failures without committing Starbytes to a full JIT architecture before the interpreter is ready.

## Sources

- CPython vectorcall fast path:
  - [python/cpython `Include/internal/pycore_call.h#L839-L868`](https://github.com/python/cpython/blob/main/Include/internal/pycore_call.h#L839-L868)
- CPython direct interpreter-frame handoff in a specialized path:
  - [python/cpython `Python/generated_cases.c.h#L3668-L3709`](https://github.com/python/cpython/blob/main/Python/generated_cases.c.h#L3668-L3709)
- V8 receiver-shaped call emission with call IC slots:
  - [v8 `src/interpreter/bytecode-generator.cc#L6290-L6307`](https://chromium.googlesource.com/v8/v8/+/master/src/interpreter/bytecode-generator.cc#6290)
- V8 feedback vector layout and call IC slot creation:
  - [v8 `src/feedback-vector.h#L105-L145`](https://chromium.googlesource.com/v8/v8/%2B/52ab610bd13/src/feedback-vector.h#105)
  - [v8 `src/feedback-vector.h#L233-L282`](https://chromium.googlesource.com/v8/v8/%2B/52ab610bd13/src/feedback-vector.h#233)
- V8 interpreter entry using feedback vector state and invocation counts:
  - [v8 `src/builtins/x64/builtins-x64.cc#L887-L919`](https://chromium.googlesource.com/v8/v8/%2B/fbb9df7218ae348b56104ff8fdd82b578e42d189/src/builtins/x64/builtins-x64.cc#887)
  - [v8 `src/builtins/x64/builtins-x64.cc#L904-L906`](https://chromium.googlesource.com/v8/v8/%2B/fbb9df7218ae348b56104ff8fdd82b578e42d189/src/builtins/x64/builtins-x64.cc#904)
