# Typed JIT VM Cache Gap Matrix

## Scope

This document compares the architecture proposed in:

- `/Users/alextopper/Downloads/typed_jit_vm_cache_architecture.md`

Against the current Starbytes implementation.

The goal is not to judge whether Starbytes should adopt the full architecture immediately. The goal is to make the gap explicit:

- what already exists,
- what only exists in a weak analogue,
- what is absent,
- and what would need to change to move toward that architecture.

## Current Runtime Position

Starbytes currently behaves like:

- an ahead-of-time compiler / semantic pipeline,
- a bytecode-style runtime interpreter,
- a small set of runtime lookup maps,
- no JIT tiers,
- no runtime code cache,
- no inline-cache or PIC dispatch layer,
- no statistical shared cache policy layer.

That means Starbytes only matches the "interpreter" prerequisite from the architecture note. Most of the cache architecture described in the note is not currently implemented.

## Matrix

| Architecture Feature | Proposed Role In The Note | Starbytes Status | Evidence In Starbytes | Gap Assessment | Recommended Follow-Up |
| --- | --- | --- | --- | --- | --- |
| Interpreter / bytecode VM | Base execution tier | Implemented | `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:860` | No gap at this layer | Keep as baseline execution engine |
| Baseline compiler tier | Runtime-compiled non-optimizing tier | Not implemented | No runtime compile tier found in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp` | Large gap | Would require runtime compiler pipeline, tier manager, and executable code ownership |
| Optimizing JIT | Hot-path optimized compiled tier | Not implemented | No JIT, deopt, patching, or compiled-method residency code found | Large gap | Separate long-term runtime project |
| Per-call-site PIC / inline caches | Monomorphic -> PIC -> megamorphic dispatch | Not implemented | Member calls resolve by hierarchy walk and function-name lookup in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:1705`, `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:2309`, `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:2318` | Large gap | First practical runtime optimization target |
| Virtual dispatch target cache | Exact receiver-type fast path | Not implemented | No call-site cache state, no receiver-type guard table, no PIC width logic | Large gap | Add site-local monomorphic cache before any polymorphic cache work |
| Interface dispatch cache | Fast interface target resolution | Not implemented | No dedicated interface dispatch cache or shared megamorphic interface table found | Large gap | Only relevant once interface-typed runtime dispatch becomes hot enough to justify it |
| Field/layout/shape caches | Cached slot offsets / layout guards | Not implemented | No shape ID, field offset cache, layout version tag, or watchpoint system found | Large gap | Needs object-layout metadata redesign first |
| Array/collection access specialization caches | Site-local fast paths for common collection kinds | Not implemented | Collection access stays interpreted in `evalExpr(...)` | Medium-large gap | Could be added after call-site PICs if runtime profiling identifies it as hot |
| Shared megamorphic dispatch cache | Compact shared fallback for polymorphic call sites | Not implemented | No megamorphic dispatch dictionary found | Large gap | Should follow after PIC design exists |
| Shared metadata cache with W-TinyLFU | Reflection/member/generic/adaptor caches | Not implemented | No W-TinyLFU-like or bounded shared metadata cache implementation found | Large gap | Not useful until Starbytes grows substantial shared runtime metadata traffic |
| Reflection/member lookup cache | Caches repeated member resolution | Not implemented | No reflection subsystem cache found | Large gap | Low priority until reflection exists or becomes performance-sensitive |
| Generic instantiation cache | Runtime cache of generic bodies/dictionaries | Not implemented in runtime | Generic specialization happens in compiler sema via type substitution in `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp:2159`, `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp:2224`, `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp:2277` | Medium conceptual gap, large runtime gap | If runtime generics are added later, unify explicit specialization, inference, and cache keys |
| Adapter / thunk cache | Shared call adapters and ABI bridges | Not implemented | No adapter-cache or thunk-cache system found | Large gap | Only relevant with JIT, FFI growth, or richer runtime callable adaptation |
| Compile-local memo tables | Per-compile bounded tables | Partially analogous on compiler side | Compiler has phase-local analysis/state, but not as named runtime cache structures | Small runtime relevance gap | Leave out of runtime roadmap; belongs to compiler optimization planning |
| Persisted profile cache across runs | Reused hotness/type/profile data | Not implemented | No persisted runtime profile store found | Large gap | Only relevant after Starbytes has runtime profiling and multiple code tiers |
| Code cache / compiled method residency | Bounded executable code cache | Not implemented | No executable-method cache, no CLOCK-Pro/ARC, no residency segmentation | Large gap | Requires JIT first |
| Deoptimization metadata cache | Supports uncommon traps/deopt re-entry | Not implemented | No deopt metadata, OSR, uncommon-trap blocks, or dependency invalidation found | Large gap | JIT prerequisite |
| Invalidation/version-tag infrastructure | Safe cache coherence for layout/type changes | Not implemented broadly | No class version tags, method patch epochs, shape versions, or dependency invalidation framework found | Large gap | Foundational prerequisite for most runtime caches |
| Native callback lookup cache | Cache of native symbol resolution | Implemented in narrow form | `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:364`, `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:497` | Small gap relative to note; this is a real cache, but very narrow | Keep; could be expanded into a broader native symbol/module lookup cache if needed |
| Runtime class/type registries | Fast class lookup by name/type | Implemented in narrow form | `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:370`, `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:371`, `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:411`, `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:423` | Small gap; useful substrate, not a hot-path dispatch cache | Good substrate for future dispatch caches |
| Runtime function lookup cache | Fast function lookup by emitted name | Not implemented efficiently | Function lookup is linear in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:476` | Medium gap | Low-risk optimization even without JIT: replace linear scan with indexed lookup |

## What Starbytes Already Has That Is Related

These are not equivalents to the architecture note, but they are relevant building blocks:

1. `nativeCallbackCache`
   - caches native callback symbol resolution
   - `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:364`
   - `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:497`

2. Runtime class registries
   - class lookup by name/type is indexed
   - `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:370`
   - `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp:371`

3. Compiler-side generic specialization
   - generic free functions and methods are specialized semantically before runtime
   - `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp:2159`
   - `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp:2224`
   - `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp:2277`

These help with future expansion, but they do not amount to the layered runtime cache architecture described in the note.

## Practical Conclusion

Starbytes does **not** currently implement the typed-JIT cache architecture in any substantial way.

The closest matches are:

- the interpreter,
- the native callback lookup cache,
- class/type registries,
- compiler-side generic specialization.

Everything else from the note should be treated as future runtime architecture work, not as functionality that is already present in a partial hidden form.

## Recommended Progression If Starbytes Moves Toward This Architecture

1. Replace linear runtime function lookup with indexed lookup
   - low risk
   - immediately useful even without JIT

2. Add site-local monomorphic caches for runtime member invocation
   - best first real step toward PIC-style dispatch

3. Add small polymorphic inline caches for method dispatch
   - only after monomorphic cache correctness/invalidation is established

4. Introduce versioned invalidation metadata
   - class version
   - method version
   - layout/version epochs if object layout grows more dynamic

5. Re-evaluate whether a shared megamorphic dispatch cache is needed
   - only after measuring dispatch diversity and miss patterns

6. Defer W-TinyLFU / CLOCK-Pro / ARC until Starbytes has:
   - shared runtime metadata worth bounding,
   - or a real JIT code cache

Without JIT tiers and deoptimization, the architecture note is mostly a roadmap for a future runtime generation, not a description of the current system.
