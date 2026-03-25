# Function Execution Handling Proposal

## Goal

Design and track a Starbytes function execution path that attacks the current Track A bottlenecks without requiring a JIT as the first move.

This document now serves two purposes:

- record the original call-path redesign proposal
- track which slices are now implemented as of March 25, 2026

The target remains a materially faster interpreter path for:

- exact-arity Starbytes function calls
- monomorphic method calls
- builtin/native calls
- small hot helper functions in numeric kernels

## Status Update

### Implemented

The current runtime now includes the following parts of the proposal:

- decoded function bodies stored on each runtime function template:
  - [`src/compiler/RTCode.cpp:647`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/RTCode.cpp#L647)
- direct in-memory function body execution via `MemoryInputStream`, rather than seek/restore on the shared module stream:
  - [`src/runtime/RTEngine.cpp:2029`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2029)
- reusable local-frame free list instead of rebuilding a fresh frame object every call:
  - [`src/runtime/RTEngine.cpp:1134`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L1134)
- direct-call opcode for semantically resolved function calls:
  - codegen: [`src/compiler/CodeGen.cpp:1743`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L1743)
  - runtime dispatch: [`src/runtime/RTEngine.cpp:2329`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2329)
- direct-arg buffering for exact direct calls, avoiding the old generic vector-building path for those sites:
  - [`src/runtime/RTEngine.cpp:2224`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2224)

### Partially implemented

- The decoded code image exists, but the runtime does not yet maintain an explicit `pc/frame_base/operand_base/caller/return_pc` frame machine.
- The frame arena goal is partially met by a free list, but not by a dedicated arena with fully separated object/numeric storage.

### Not yet implemented

- call-site feedback table for calls
- quickening from dynamic calls to direct/member-specialized calls
- builtin member ids and dedicated builtin member opcodes
- direct method slot caching for user-defined methods
- full removal of scope-map bookkeeping from fast-call eligible functions
- leaf-function quickening/inlining

## Current Signal

### Latest benchmark evidence

Latest measured report:

- [`benchmark/results/summaries/track_a_steady-state_20260325T230531Z.report.md`](/Users/alextopper/Documents/GitHub/starbytes-project/benchmark/results/summaries/track_a_steady-state_20260325T230531Z.report.md)

Compared against the degraded call-path baseline:

- `benchmark/results/raw/track_a_starbytes_steady-state_20260325T182048Z`

Key Starbytes results:

| workload | degraded baseline(s) | current(s) | change |
|---|---:|---:|---:|
| binary-trees | 196.812 | 28.104 | 7.00x faster |
| spectral-norm | 41.044 | 16.177 | 2.54x faster |
| n-body | 1.396 | 1.431 | 2.5% slower |
| k-nucleotide | 0.606 | 0.609 | roughly flat |
| fasta | 0.520 | 0.381 | 1.37x faster |
| regex-redux | 0.031 | 0.028 | 1.09x faster |

Interpretation:

- the function-call redesign work landed real wins in `binary-trees` and `spectral-norm`
- `n-body` is still mostly an index-access problem
- `k-nucleotide` is still primarily a member/builtin dispatch problem

### Latest runtime profile signal

Current important profiles:

- [`steady-state_binary-trees.runtime-profile.json`](/Users/alextopper/Documents/GitHub/starbytes-project/benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_binary-trees.runtime-profile.json)
- [`steady-state_spectral-norm.runtime-profile.json`](/Users/alextopper/Documents/GitHub/starbytes-project/benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_spectral-norm.runtime-profile.json)
- [`steady-state_k-nucleotide.runtime-profile.json`](/Users/alextopper/Documents/GitHub/starbytes-project/benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_k-nucleotide.runtime-profile.json)

Observed signal:

- `binary-trees`
  - total runtime dropped from about `200990 ms` to about `30985 ms`
  - `makeTree` and `checkTree` both fell sharply
  - `CODE_RTCALL_DIRECT` replaced `CODE_RTIVKFUNC`/`CODE_RTFUNC_REF` for the hot recursive calls
  - remaining large cost: member access
- `spectral-norm`
  - total runtime dropped from about `41638 ms` to about `16146 ms`
  - hot helper `A` dropped from about `29159 ms` to about `2272 ms`
  - remaining large cost is no longer generic function entry
- `k-nucleotide`
  - member access is still about `357.839 ms`
  - `CODE_RTMEMBER_IVK` still executes `15125` times
  - direct function calls are negligible there

The current conclusion is different from the original March 25 baseline:

- generic function entry is no longer the primary runtime emergency
- member/builtin dispatch is now the clearest next call-path bottleneck

## External Grounding

### CPython

CPython uses a dedicated fast-call ABI instead of forcing all calls through tuple/dict packaging:

- `_PyObject_VectorcallTstate(...)` receives a raw argument array and only falls back to `tp_call` when no vectorcall entry exists:
  - [pycore_call.h#L839-L868](https://github.com/python/cpython/blob/main/Include/internal/pycore_call.h#L839-L868)

CPython also has specialized interpreter paths that push a new interpreter frame directly and continue dispatch in the callee instead of bouncing through a generic object-call slow path:

- `BINARY_OP_SUBSCR_GETITEM` caches a Python function target, pushes `_PyFrame_PushUnchecked(...)`, then `DISPATCH()` continues in the new frame:
  - [generated_cases.c.h#L3668-L3709](https://github.com/python/cpython/blob/main/Python/generated_cases.c.h#L3668-L3709)

The important CPython lesson is:

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

The important V8 lesson is:

- classify call shapes early
- attach call-site feedback to them
- let feedback drive specialization of hot call sites

## Proposal Status by Item

## 1. Replace the current generic call path with a stack-span fast call ABI

### Status

Partially implemented.

What landed:

- direct calls now evaluate args into a contiguous VM-owned buffer before entry
- exact known direct calls no longer require a `StarbytesFuncRef` object at the call site

What has not landed:

- no single `enterCall(...)`-style unified call entry point yet
- Starbytes, builtin, and native paths are still split across different helpers
- function entry still builds a scope string and still touches the allocator path

## 2. Stop using `std::istream` position switching as the primary callee-entry mechanism

### Status

Mostly implemented for function entry.

What landed:

- function bodies are decoded and stored in `RTFuncTemplate::decodedBody`
- function execution now uses an in-memory input stream over that body
- direct function entry no longer seeks into the shared module stream and restores afterward

What remains:

- the runtime still uses stream-style decoding inside the function body
- the interpreter is not yet running on an explicit PC/frame machine

## 3. Split one broad call opcode into explicit call shapes

### Status

Partially implemented.

What landed:

- `CODE_RTCALL_DIRECT`

What remains:

- no direct builtin-member opcode
- no direct method opcode
- no closure-specific opcode
- `CODE_RTIVKFUNC` is still the fallback generic dynamic-call path
- `CODE_RTMEMBER_IVK` is still too broad

## 4. Add per-call-site feedback and quickening for calls

### Status

Not implemented.

The runtime has expression quickening for some numeric/index cases, but it does not yet have call-site feedback or call quickening.

## 5. Introduce a reusable frame arena instead of per-call frame rebuilding

### Status

Partially implemented.

What landed:

- reusable frame free list for `LocalFrame`

What remains:

- dedicated arena
- inline small-frame storage policy
- stronger separation between object slots and typed numeric slots at frame allocation level

## 6. Move `self` and locals fully into frame slots, not scope maps

### Status

Not implemented.

The runtime still constructs a function scope string and still installs `self` through allocator-backed scope state:

- [`src/runtime/RTEngine.cpp:2065`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2065)

## 7. Give member calls direct method slots and builtin ids

### Status

Not implemented here.

This is now the clearest next slice. The follow-on planning for that work lives in:

- [`docs/MemberBuiltinDispatchPlan.md`](/Users/alextopper/Documents/GitHub/starbytes-project/docs/MemberBuiltinDispatchPlan.md)

## 8. Add an exact-arity leaf-function fast path

### Status

Not implemented as a dedicated feature.

The current `CALL_DIRECT` plus decoded-body path already captures much of the gross overhead that made `spectral-norm.A` expensive, but there is no leaf-specific quickening or inline expansion yet.

## Proposed Runtime Shape

The updated proposal shape is now:

### Completed base

1. Evaluate direct-call args into a contiguous temporary buffer.
2. Enter decoded in-memory function bodies directly.
3. Reuse frames through a free list.

### Remaining fast-path work

1. Add call-site feedback for remaining dynamic call shapes.
2. Split member dispatch into:
   - builtin member opcodes
   - direct field-slot access
   - cached user-method dispatch
3. Remove scope-map overhead from slot-friendly functions.
4. Add leaf-function quickening only if the post-member-dispatch profile still justifies it.

## Why this still matters

### binary-trees

The original function-call redesign worked:

- recursive call overhead dropped dramatically

The remaining large cost is now:

- `CODE_RTMEMBER_GET`
- `CODE_RTMEMBER_SET`

So the next relevant item is proposal item 7, not more generic call reshaping.

### spectral-norm

The original call-path redesign worked:

- helper `A` no longer dominates at the old level

Leaf-only call quickening is still possible later, but it is no longer the most urgent runtime project.

### n-body

This is still mostly typed index access and numeric loop execution. This proposal was never the whole answer there.

### k-nucleotide and fasta

These still point directly at builtin/member dispatch and are the strongest argument for finishing proposal item 7.

## Thin-Slice Rollout

### Phase 1 [Implemented]

Implemented:

- decoded code image
- frame free-list reuse
- `CALL_DIRECT`
- exact direct Starbytes function entry through decoded bodies

Observed result:

- `binary-trees` and `spectral-norm` improved materially against the degraded baseline

### Phase 2 [Next]

Implement:

- builtin member ids for hot string/array/dict/regex methods
- dedicated builtin member opcode(s)
- direct field-slot get/set where semantic/codegen can prove the target

Success criteria:

- `k-nucleotide` improves materially
- `binary-trees` member-access time drops materially
- runtime profiles show lower `CODE_RTMEMBER_GET`/`SET`/`IVK` usage

### Phase 3 [Later]

Implement:

- call-site feedback table
- dynamic-to-direct call quickening
- monomorphic user-method cache
- optional leaf-function quickening if still justified

Success criteria:

- remaining fallback counts become visible and actionable in runtime profiles

## Validation

Track with:

- runtime profile JSON already emitted by the driver
- opcode counts for:
  - `CODE_RTIVKFUNC`
  - `CODE_RTCALL_DIRECT`
  - `CODE_RTMEMBER_GET`
  - `CODE_RTMEMBER_SET`
  - `CODE_RTMEMBER_IVK`

Benchmark gates should continue to emphasize:

- `binary-trees`
- `spectral-norm`
- `k-nucleotide`

## Recommendation

Do not spend the next runtime slice re-solving generic function entry.

That slice already paid off. The proposal should now be interpreted as:

1. keep the implemented direct-call and decoded-body work
2. shift follow-on effort to member/builtin dispatch specialization
3. revisit call-site feedback and leaf quickening only after member dispatch is cheaper

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
