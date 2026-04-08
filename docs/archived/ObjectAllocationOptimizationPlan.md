# Object Allocation Optimization Plan

## Purpose

This document proposes runtime object-allocation optimizations for Starbytes to reduce:

- heap allocation count
- retained memory
- reference-count churn
- per-object property lookup overhead

The goal is practical runtime improvement on real workloads, especially:

- object-heavy recursive kernels like `binary-trees`
- container-heavy workloads
- string-heavy stdlib/module code

This is based on the current runtime object model in:

- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTObject.c`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/docs/NumericalExecutionImprovementPlan.md`

## Current Runtime Allocation Shape

## 1. Every runtime object is heap allocated

`StarbytesObjectNew(...)` in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTObject.c` allocates a heap object header for every runtime value.

Current object header includes:
- `type`
- `refCount`
- `nProp`
- `props`
- `privData`
- `freePrivData`

Implication:
- even very small values pay object-header cost
- builtins with fixed payloads still pay separate header + payload allocation patterns

## 2. Many builtin values pay two heap allocations

Examples in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTObject.c`:
- `StarbytesNumNew(...)`
  - allocates object header
  - allocates `StarbytesNumPriv`
- `StarbytesBoolNew(...)`
  - allocates object header
  - allocates `StarbytesBoolPriv`
- `StarbytesFuncRefNew(...)`
  - allocates object header
  - allocates `StarbytesFuncRefPriv`
- `StarbytesTaskNew(...)`
  - allocates object header
  - allocates `StarbytesTaskPriv`

Implication:
- common scalar and helper objects are over-allocated
- refcounted numeric churn remains expensive when code falls off typed-local fast paths

## 3. Strings and arrays store metadata as object properties

Examples:
- `String.length` is stored as a `StarbytesNum` property object
- `Array.length` is stored as a `StarbytesNum` property object
- `Dict.length`, `Dict.keys`, and `Dict.values` are all regular object properties

Implication:
- builtin container metadata pays:
  - property-table storage
  - object lookup cost
  - extra retained objects
- even trivial metadata updates mutate secondary heap objects instead of plain fields

## 4. Properties are linear arrays of name/data pairs

`StarbytesObjectAddProperty(...)` and `StarbytesObjectGetProperty(...)` use a linear array.

Implication:
- class field lookup is linear in field count
- object construction repeatedly appends properties
- instance member get/set in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp` walks a generic property model even for statically-known class fields

## 5. Class instances are generic property bags

Class construction in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp` creates a generic class object, then adds fields as named properties.

Implication:
- each field is stored through a property table rather than a fixed slot layout
- field reads/writes pay string-name lookup cost
- object-heavy kernels like `binary-trees` pay this on every node allocation and tree walk

## 6. Dict is not a hash table

`Dict` currently stores:
- `keys: Array`
- `values: Array`
- `length: Num`

Lookup is linear in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTObject.c`.

Implication:
- poor lookup scaling
- unnecessary key/value object churn for common dictionary use
- memory overhead from multi-object composition

## 7. Numeric arrays are improved, but cache boxing still exists

Typed numeric arrays are already implemented and help significantly.

But boxed cache materialization still exists for generic API paths:
- numeric arrays maintain a per-element boxed cache when indexed through the generic object API

Implication:
- mixed generic access can still allocate transient numeric wrappers
- this is much better than fully boxed arrays, but still not minimal

## 8. Strings allocate eagerly for many short-lived operations

Current runtime creates new strings for:
- substring/slice results
- scalar indexing results
- many builtin string methods
- concatenation
- regex outputs

Implication:
- short-lived string allocation is likely a measurable cost in workloads like `fasta`
- scalar-correct indexing improved correctness, but still allocates a fresh string object for each extracted scalar

## Where This Likely Hurts Most

## `binary-trees`

This benchmark is not primarily numeric. It is:
- recursive
- allocation-heavy
- field-access-heavy

Likely dominant costs:
- node allocation count
- generic property-bag field storage
- recursive object/member overhead
- optional/secure path cost in the current kernel implementation

## `fasta`

Likely dominant costs:
- many tiny string allocations
- repeated substring/slice creation
- generic string object overhead

## General stdlib and module code

Likely dominant costs:
- refcount churn on tiny objects
- object-property metadata overhead for builtin containers
- dictionary linear lookup

## Design Principles For Optimization

1. Preserve language semantics first.
   - Starbytes values remain reference-counted objects at the language level unless a specific optimization proves invisible.

2. Optimize the common runtime shapes, not every shape equally.
   - numbers, bools, strings, arrays, dicts, and class instances matter most.

3. Reduce allocation count before chasing allocator micro-optimizations.
   - the best allocation is the one not performed.

4. Specialize builtin/runtime-known structures before inventing a full GC or moving collector.
   - Starbytes can get large wins without replacing the current ownership model immediately.

5. Favor fixed-layout storage for statically-known objects.
   - generic property bags should be the fallback, not the default for class instances.

## Garbage Collection Strategy

## Current collector shape

Starbytes currently uses immediate reference counting:

- `StarbytesObjectReference(...)`
- `StarbytesObjectRelease(...)`

in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTObject.c`.

That has two main costs:

1. runtime cost
   - every ownership edge update pays increments and decrements
   - object-heavy code spends time maintaining liveness, not doing useful work
2. structural limitation
   - plain reference counting does not reclaim cycles without extra machinery

For Starbytes' current workload mix, the runtime cost is the bigger problem.

## Would a different GC be faster?

Yes.

For Starbytes, a well-designed tracing collector should be faster than immediate reference counting on:

- allocation-heavy kernels like `binary-trees`
- container-heavy code
- string-heavy workloads with many short-lived temporaries

The reason is simple:

- most runtime objects die young
- tracing collectors are good at reclaiming many young objects cheaply
- immediate refcounting pays overhead on every edge mutation whether the object survives or not

## Recommended target collector

The best long-term fit for Starbytes is a:

### hybrid generational collector

with:

- a young generation designed for very cheap allocation
- a non-moving or pinned old generation for ABI stability
- explicit support for native-visible objects

Recommended shape:

### Young generation

- bump-pointer allocation
- collected frequently
- optimized for short-lived temporaries
- allowed to evacuate or compact only when the object is not native-pinned

### Old generation

- non-moving mark-sweep
- used for long-lived objects:
  - module globals
  - long-lived arrays/strings
  - class instances that survive promotion
  - native-visible or pinned objects

### Pinned/native-visible objects

- any object exposed through the native-module ABI can be pinned or allocated directly in old space
- this preserves raw-pointer safety for current native modules

## Why not a fully moving GC first?

Because the current ABI in `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/interop.h` exposes raw `StarbytesObject` pointers directly.

That means a fully moving collector would require:

- handle indirection
- strict pinning rules
- or a redesigned native ABI

That is possible, but it is a much larger platform change than Starbytes needs for the next wave.

## Why not keep reference counting and only optimize it?

There is a lower-risk interim option:

### deferred/coalesced reference counting

That would:

- avoid some redundant inc/dec traffic
- batch decrements
- reduce churn on stack/local references

But it still leaves Starbytes paying the core reference-counting tax across the runtime.

So:

- deferred RC is a good transition step
- it is not the best end-state if runtime throughput is the main objective

## Recommended delivery path

## Phase GC-1: collector groundwork

Before changing collection policy, add the runtime metadata a tracing collector needs:

- object graph traversal hooks by runtime type
- root enumeration for:
  - local frames
  - globals
  - class registries
  - native module registries
  - microtask queues
  - pending task values/errors
- allocation counters
- per-type live-object counters

This phase should not change collection semantics yet.

## Phase GC-2: deferred reference counting

Introduce a lower-risk intermediate collector layer:

- defer decrements for frame-local and temporary ownership changes
- coalesce redundant inc/dec pairs
- keep object destruction semantics compatible with today
- optionally add a simple cycle detector later if needed

Why do this first:

- reduces immediate runtime cost
- hardens root/ownership accounting
- provides a safer stepping stone toward tracing GC

## Phase GC-3: nursery for short-lived objects

Add a young generation for runtime-created temporary objects:

- bump allocation
- frequent minor collections
- promote survivors after a small number of collections

Allocation policy:

- objects that are clearly runtime-local and not native-exposed start in nursery
- objects that are native-visible or explicitly pinned bypass nursery

This is the phase most likely to give a clear speedup on allocation-heavy workloads.

## Phase GC-4: old-generation mark-sweep

Move long-lived managed objects to a non-moving old generation:

- mark from runtime roots
- sweep unreachable objects
- preserve stable addresses for old-generation objects

This phase reduces dependence on refcounts for long-lived structures and allows the runtime to stop doing immediate release work on every reference mutation.

## Phase GC-5: optional cycle cleanup and RC retirement

At this point Starbytes can choose one of two end-states:

### Option A: hybrid collector

- keep narrow RC only where deterministic teardown is useful
- use tracing GC for most managed objects

### Option B: tracing-first collector

- retire general immediate RC
- keep pinning/handles for native interop

For Starbytes, Option A is the safer medium-term target.

## Expected performance impact

The biggest GC-driven wins would likely come from:

1. cheaper allocation for young temporary objects
2. removing inc/dec churn from hot loops and object graph updates
3. avoiding deep recursive release cascades during object teardown

This should help:

- `binary-trees`
- object-heavy module/stdlib code
- string-heavy temporary paths

It would help numeric kernels too, but less than object-heavy kernels, because the typed numeric fast paths already avoid part of the boxing cost.

## Major risks

1. Native ABI safety
   - raw object pointers must remain valid for native modules
2. Root enumeration bugs
   - a tracing collector is only as good as its root map
3. Semantic surprises
   - timing of `freePrivData` execution becomes less deterministic
4. Scope creep
   - collector work can easily turn into a runtime rewrite if not phased carefully

## Recommendation

The fastest realistic garbage-collection direction for Starbytes is:

### a hybrid generational collector

with:

- deferred RC as the transition step
- nursery allocation for short-lived runtime objects
- non-moving old generation for promoted and native-visible objects

That is more likely to deliver real speedups than trying to perfect immediate reference counting, while still respecting the current Starbytes runtime and native-module model.

## Recommended Phases

## Phase 1: Inline builtin payloads into the object header [Implemented]

### Goal

Remove second allocations for fixed-size builtin payloads.

### Work

Refactor builtin object representation so these types store payload inline in the object header instead of via `privData`:
- `Num`
- `Bool`
- `FuncRef`
- likely `Task` header fields except dynamic error text/value payloads

Possible direction:
- add a tagged inline payload union to `_StarbytesObject`
- keep `privData` only for variable-size or uncommon payloads

### Why this matters

Today a numeric object commonly pays:
- one allocation for object header
- one allocation for numeric payload

That is too expensive for fallback arithmetic, boxed array cache values, metadata length values, and transient conversions.

### Expected impact

- lower allocation count for boxed numerics
- lower allocator traffic
- lower memory fragmentation
- immediate improvement across many runtime paths

### Risk

Low-to-medium.

This is structural but localized to runtime object representation.

### Status

- implemented
- `Num`, `Bool`, `FuncRef`, and `Task` now store their fixed-size payloads inline in `_StarbytesObject`
- numeric wrappers no longer allocate a second heap block for `StarbytesNumPriv`
- bool/function-ref/task objects no longer allocate separate private payload structs
- task teardown still preserves dynamic payload cleanup for retained result values and error strings

## Phase 2: Stop storing builtin metadata as generic object properties [Implemented]

### Goal

Move builtin metadata into dedicated struct fields.

### Work

Eliminate property-backed metadata for builtins such as:
- `String.length`
- `Array.length`
- `Dict.length`
- `Dict.keys`
- `Dict.values`

Instead:
- store length/count directly in builtin private structs
- expose property-like behavior through runtime member dispatch when needed

### Why this matters

This removes extra heap objects and avoids property lookup for builtin metadata that is already runtime-known.

### Expected impact

- fewer allocations per string/array/dict
- less refcount churn
- faster builtin member access
- lower memory footprint for all container values

### Risk

Low.

Semantics can remain unchanged because the runtime already handles builtin members specially in many places.

### Status

- implemented
- `String.length`, `Array.length`, and `Dict.length` are now stored as direct runtime metadata instead of generic object properties
- `Dict.keys` and `Dict.values` now live in dedicated dict storage instead of generic object properties
- runtime member access now synthesizes builtin `length` reads from direct metadata
- native/runtime helper code now uses dedicated dict getters instead of property-table lookup

## Phase 3: Replace class instance property bags with field-slot layouts [Implemented]

### Goal

Make class instances fixed-layout objects when the class layout is known.

### Work

For runtime class metadata, assign each field a stable slot index.

Store instance fields in a contiguous slot array instead of generic named properties.

Add runtime helpers for:
- field get by slot
- field set by slot
- optional fallback to named property lookup only for dynamic/legacy cases

### Why this matters

This is the highest-leverage object-model optimization for kernels like `binary-trees`.

Right now each node instance pays:
- generic object allocation
- property-array growth for fields
- linear name lookup for field access

A slot-based instance turns that into:
- object allocation
- contiguous field storage
- constant-time indexed access

### Expected impact

High.

This should directly improve:
- `binary-trees`
- class-heavy programs
- method/field access throughput
- object memory density

### Risk

Medium.

This touches instance construction and field/member runtime behavior, but it is still conceptually straightforward.

### Status

- implemented
- runtime class metadata now builds stable inherited field-slot layouts
- class instances now allocate contiguous field storage when the class layout is known
- runtime member reads/writes for class fields now resolve through slot access instead of linear property lookup
- generic named-property fallback remains available for dynamic or legacy cases

## Phase 4: Add a small-object allocator / slab allocator

### Goal

Make remaining heap allocations cheaper.

### Work

Introduce size-class pools for the most common runtime objects:
- object headers
- small strings
- small arrays / array metadata
- maybe tasks and regex objects if usage justifies it

Possible model:
- per-type or per-size freelists
- thread-local or interpreter-local slabs
- bulk free only when refcount reaches zero, with recycled storage returned to freelists

### Why this matters

Even after representation cleanup, Starbytes will still allocate many short-lived runtime objects.

A general allocator cannot exploit Starbytes object lifetimes and type regularity as well as a runtime-local pool can.

### Expected impact

Medium.

This will not fix structural overhead by itself, but it will make the remaining unavoidable allocations cheaper.

### Risk

Medium.

Memory management correctness must stay exact.

## Phase 5: String representation improvements

### Goal

Reduce short-lived string allocation and per-string overhead.

### Work

Recommended order:

1. Add small-string optimization for short UTF-8 strings.
   - store short payload inline in the object
2. Intern stable/runtime-hot strings where it is safe.
   - field names
   - builtin method names
   - maybe frequently repeated literals
3. Add string-view or slice-view objects for internal runtime operations.
   - only if lifetime rules can be made safe under current refcounting
4. Add specialized APIs that avoid temporary string objects in tight loops.
   - especially for scalar iteration and search paths

### Why this matters

`fasta` and text-heavy stdlib paths create many short-lived strings. Current scalar-correct indexing returns a new string object each time.

### Expected impact

Medium-to-high for text-heavy workloads.

### Risk

Medium.

String views are powerful but easy to get wrong with ownership and API exposure. Small-string optimization is lower risk and should come first.

## Phase 6: Replace Dict with a real hash table

### Goal

Reduce both memory and time overhead for dictionary operations.

### Work

Replace `Dict`’s current parallel-array representation with a hash table layout.

Preferred shape:
- open addressing or robin-hood hashing
- inline metadata for size/capacity
- contiguous entries
- string and numeric key fast paths

### Why this matters

Current `Dict` is both time-inefficient and allocation-heavy.

### Expected impact

High for dictionary-heavy programs.

### Risk

Medium.

The semantics are straightforward, but implementation quality matters.

## Phase 7: Escape analysis and stack/region allocation for short-lived objects

### Goal

Avoid heap allocation for objects that provably do not escape.

### Work

Start with compiler/runtime cooperation for narrow cases:
- short-lived temporary arrays in expression-local contexts
- non-escaping wrapper objects
- maybe constructor-local short-lived helper objects

A simpler first model is region allocation per runtime frame or per statement block for known non-escaping temporaries.

### Why this matters

This is where a runtime can remove whole classes of allocations instead of making them cheaper.

### Expected impact

Potentially high, but only after narrow safe slices exist.

### Risk

High.

This is not a first-wave change.

## Phase 8: Tagged immediates for tiny scalars

### Goal

Eliminate object allocation entirely for the hottest scalar cases.

### Work

Explore tagged-pointer or NaN-box style representation for:
- `null`
- `Bool`
- small `Int`
- maybe `FuncRef` or class tags if the representation stays sane

### Why this matters

This can remove the highest-frequency scalar allocations entirely.

### Expected impact

Very high if implemented well.

### Risk

Very high.

This would touch nearly every runtime API and should not happen before the lower-risk structural wins above.

## Recommended Priority Order

1. Phase 2: builtin metadata out of generic properties
2. Phase 3: field-slot layouts for class instances
3. Phase 1: inline builtin payloads
4. Phase 4: small-object allocator
5. Phase 5: string representation improvements
6. Phase 6: real hash-table `Dict`
7. Phase 7: escape analysis / region allocation
8. Phase 8: tagged immediates

## Why this order

- builtin metadata and instance field slots attack the current hottest structural waste directly
- inline payloads reduce obvious scalar over-allocation broadly
- allocator pooling helps after representation cleanup
- strings and dicts deserve dedicated work, but after the object model is less wasteful
- tagged immediates are powerful, but too invasive for an early step

## Thin Vertical Slice Recommendation

If only one optimization is done first, it should be:

### Fixed-layout class instances with slot-based fields

Reason:
- it directly targets the object-heavy path seen in `binary-trees`
- it reduces both memory and CPU cost
- it aligns with Starbytes’ existing compile-time knowledge of class fields
- it does not require a new collector or a whole runtime representation rewrite

A good first benchmark target after implementation would be:
- `/Users/alextopper/Documents/GitHub/starbytes-project/benchmark/languages/starbytes/track_a/binary_trees.starb`

## Suggested Validation Matrix

For each phase, measure:
- total allocations if runtime counters are added
- peak resident memory
- object count by type if counters are added
- Track A impact on:
  - `binary-trees`
  - `fasta`
  - `n-body`
- full test suite stability

Recommended instrumentation additions:
- object allocations by runtime type
- property lookups by type
- string allocations by byte length bucket
- array materialize-boxed transitions
- dict probe counts once hash-table dict lands

## Concrete Near-Term Recommendations

### 1. Stop representing `length` as a `StarbytesNum` property on builtin types

This is low-risk and broadly beneficial.

### 2. Add slot-based class instance field storage

This is likely the biggest real win for object-heavy code.

### 3. Inline `Num` and `Bool` payloads into the object header

This will reduce scalar-object churn in generic paths.

### 4. Add runtime allocation counters before and after each phase

Without this, memory and allocation improvements will be hard to attribute correctly.

## Non-Goals For This Plan

This document does not propose:
- replacing the runtime with a tracing GC immediately
- a full moving collector
- a JIT-specific object model
- behavioral changes to Starbytes language semantics

Those may become relevant later, but the current runtime can get large wins first from object-model specialization and allocation reduction alone.
