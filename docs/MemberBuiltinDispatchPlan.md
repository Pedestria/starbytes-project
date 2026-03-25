# Member/Builtin Dispatch Plan

## Goal

Reduce the remaining runtime cost in Starbytes member access and builtin member invocation after the direct-call and decoded-code-image work.

This plan targets the current hot paths in:

- `CODE_RTMEMBER_GET`
- `CODE_RTMEMBER_SET`
- `CODE_RTMEMBER_IVK`

and is intended to move:

- `binary-trees`, where field reads/writes still dominate
- `k-nucleotide`, where builtin string/array member calls still dominate
- follow-on collection-heavy kernels that will hit the same dispatch shape

## Current State

### Codegen shape

Codegen still emits all instance method calls through one generic member-invoke opcode:

- [`src/compiler/CodeGen.cpp:1691`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L1691)

That means codegen currently does not distinguish:

- builtin string/array/dict/regex member calls
- user-defined class method calls
- field-like metadata access such as `.length`

### Runtime member-get path

The current member-get path:

- decodes the member name as a string
- handles builtin metadata checks by string comparison
- falls back to class field slot lookup
- then falls back again to dynamic property lookup

Relevant code:

- [`src/runtime/RTEngine.cpp:3468`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3468)

### Runtime member-set path

The current member-set path:

- decodes the member name as a string
- guards builtin metadata names with string comparisons
- checks field slot lookup
- otherwise linearly scans object properties

Relevant code:

- [`src/runtime/RTEngine.cpp:3510`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3510)

### Runtime member-invoke path

`CODE_RTMEMBER_IVK` is currently doing too much in one opcode:

1. evaluate receiver
2. decode method name string
3. collect arguments into `std::vector<StarbytesObject>`
4. branch on receiver runtime kind
5. dispatch builtin methods by repeated string comparisons
6. for user methods, walk the class hierarchy and resolve the method by mangled name
7. finally call through the generic function entry path

Relevant code:

- [`src/runtime/RTEngine.cpp:3563`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3563)
- builtin string methods start at [`src/runtime/RTEngine.cpp:3649`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3649)
- builtin array methods start at [`src/runtime/RTEngine.cpp:3906`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3906)
- builtin dict methods start at [`src/runtime/RTEngine.cpp:4121`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4121)
- user class method lookup starts at [`src/runtime/RTEngine.cpp:4238`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4238)

## Current Runtime Signal

From the latest Track A runtime profiles under `benchmark/results/raw/track_a_steady-state_20260325T230531Z`:

### binary-trees

- member access time is still about `140614.872 ms`
- `CODE_RTMEMBER_GET` executes `3,973,147` times
- `CODE_RTMEMBER_SET` executes `3,962,232` times

Interpretation:

- `binary-trees` is now much faster overall, but object field traffic is still a large remaining tax
- the next win here is not generic function entry; it is direct field/member handling

### k-nucleotide

- member access time is about `357.839 ms`
- `CODE_RTMEMBER_IVK` executes `15,125` times
- `CODE_RTMEMBER_GET` executes `9,320` times
- only `15` direct function calls occur

Interpretation:

- `k-nucleotide` is still overwhelmingly a member-dispatch problem
- builtin calls like string and array helpers are more important here than function-call specialization

### fasta

- member access time is much smaller at about `7.678 ms`
- `CODE_RTMEMBER_GET` still executes `4,003` times

Interpretation:

- `fasta` is less blocked on member invocation than `k-nucleotide`
- field/metadata access cleanup still helps, but builtin invoke specialization matters less here

## Design Principles

1. Split field access, builtin member calls, and user method calls into separate shapes.
2. Encode compact ids in bytecode when codegen already knows the target shape.
3. Keep generic dynamic member lookup as fallback, not as the default.
4. Prefer enum dispatch and slot lookup over repeated string comparisons.
5. Preserve current language behavior first; optimize the common shapes second.

## Proposed Runtime Shape

## 1. Separate member access into explicit bytecode families

Replace the current broad member opcodes with more explicit shapes.

### Proposed read/write opcodes

- `MEMBER_GET_FIELD slot`
- `MEMBER_SET_FIELD slot`
- `MEMBER_GET_BUILTIN builtin_member_id`
- `MEMBER_SET_DYNAMIC name_id`
- `MEMBER_GET_DYNAMIC name_id`

### Proposed invoke opcodes

- `CALL_BUILTIN_MEMBER builtin_member_id argc`
- `CALL_METHOD_DIRECT method_id argc`
- `CALL_METHOD_DYNAMIC name_id argc`

The point is not to eliminate dynamic behavior. The point is to keep the generic path only for the cases that are actually dynamic.

## 2. Add builtin member ids

Define a compact runtime enum for hot builtin members.

Initial coverage should include the methods already implemented in `RTEngine.cpp`:

### String

- `isEmpty`
- `at`
- `slice`
- `contains`
- `startsWith`
- `endsWith`
- `indexOf`
- `lastIndexOf`
- `lower`
- `upper`
- `trim`
- `replace`
- `split`
- `repeat`

### Array

- `isEmpty`
- `push`
- `pop`
- `at`
- `set`
- `insert`
- `removeAt`
- `clear`
- `contains`
- `indexOf`
- `slice`
- `join`
- `copy`
- `reverse`

### Dict

- `isEmpty`
- `has`
- `get`
- `remove`
- `set`
- `keys`
- `values`
- `clear`
- `copy`

### Regex

- `match`
- `findAll`
- `replace`

### Metadata members

- `length`
- `keys`
- `values`

These ids should be declared in runtime bytecode metadata, not encoded as ad hoc strings.

## 3. Add codegen-side builtin member recognition

When codegen sees a member expression whose method name is in the builtin-id table, it should emit the builtin-member opcode instead of `CODE_RTMEMBER_IVK`.

Initial codegen rule:

- if the callee is `receiver.member(...)`
- and `member` is one of the builtin member names above
- emit `CALL_BUILTIN_MEMBER builtin_member_id argc`

Important constraint:

- do not require static receiver type proof for the first slice
- the opcode can still validate receiver kind at runtime

That keeps the first implementation low-risk while still removing string-dispatch overhead.

## 4. Dispatch builtin members by receiver-kind switch plus enum id

At runtime, builtin member invocation should become:

1. evaluate receiver
2. read builtin id
3. switch on receiver runtime kind
4. switch on builtin id
5. execute the handler

That removes:

- repeated string construction of `methodName`
- repeated chains of `if(methodName == "...")`
- repeated ambiguity about whether the call is builtin or user-defined

## 5. Stop allocating `std::vector<StarbytesObject>` for common builtin calls

Builtin member calls should use the same style as the phase-1 direct function path:

- inline arg buffer for small arities
- no heap allocation for arities `0`, `1`, or `2`
- common fast helpers for:
  - no-arg builtin
  - one-arg builtin
  - two-arg builtin

This matters because many hot builtins are:

- arity `0`: `isEmpty`, `lower`, `upper`, `trim`, `clear`, `copy`, `keys`, `values`
- arity `1`: `push`, `pop`-like indexed reads, `contains`, `get`, `split`, `repeat`
- arity `2`: `slice`, `replace`, `set`, `insert`

## 6. Add direct field slot opcodes for class fields

For object fields that semantic/codegen can resolve to class fields, emit direct field-slot bytecode instead of generic member get/set.

Target shape:

- field read: slot id is known at compile time
- field write: slot id is known at compile time

This is the highest-leverage follow-on for `binary-trees`, where field traffic is extremely high.

Important constraint:

- this should only apply to real class fields
- dynamic properties on arbitrary objects must still use the generic property path

## 7. Add monomorphic method caches only after builtin ids land

For user-defined method calls, the next step should be a small call-site cache:

- `receiver_class_id -> RTFuncTemplate*`

Guard:

- current receiver class matches cached class id

Fallback:

- do the current hierarchy lookup
- update cache

This should come after builtin-member ids because:

- `k-nucleotide` is builtin-heavy, not user-method-heavy
- builtin ids are simpler and lower-risk
- field-slot direct access is more important than method caching for `binary-trees`

## Implementation Phases

## Phase A: Builtin member ids and runtime dispatch split

Implement:

- builtin member id enum
- new builtin-member invoke opcode
- runtime dispatch table using enum ids
- inline arg buffers for builtin invocation
- profile output for builtin-member opcode counts

Success criteria:

- `k-nucleotide` runtime profile shows a drop in `member_access`
- `CODE_RTMEMBER_IVK` count falls materially in builtin-heavy workloads
- no semantic changes to existing stdlib-like builtin behavior

## Phase B: Direct field slot get/set

Implement:

- codegen recognition of direct class field access
- `MEMBER_GET_FIELD` and `MEMBER_SET_FIELD`
- runtime slot-based field load/store path

Success criteria:

- `binary-trees` shows a meaningful drop in `member_access`
- `CODE_RTMEMBER_GET` and `CODE_RTMEMBER_SET` fall materially there

## Phase C: Monomorphic user-method call cache

Implement:

- per-call-site receiver-class cache
- direct `RTFuncTemplate*` reuse after guard success
- generic hierarchy fallback on cache miss

Success criteria:

- user-method-heavy object workloads see lower member-dispatch time
- cache hit/miss counters confirm the specialization is real

## Validation Strategy

After each phase:

1. build `starbytes`
2. run runtime profile capture for:
   - `binary-trees`
   - `k-nucleotide`
   - `fasta`
3. compare against the previous profile with `tools/driver/profile/compare_runtime_profiles.py`
4. run the full Track A benchmark sweep

Primary metrics:

- runtime profile total runtime
- `member_access` subsystem time
- opcode counts for member get/set/invoke opcodes
- field-heavy and builtin-heavy kernel times

## Recommended Order

Implement in this order:

1. builtin member ids and `CALL_BUILTIN_MEMBER`
2. direct field-slot get/set
3. monomorphic user-method caches

Reasoning:

- builtin member dispatch is the clearest next win for `k-nucleotide`
- direct field-slot access is the clearest next win for `binary-trees`
- user-method caching matters, but it is not the first-order bottleneck in the current Track A profiles

## Non-goals

- no full dynamic inline cache framework in the first slice
- no broad reflection redesign
- no attempt to make dynamic property bags as cheap as static fields
- no attempt to solve collection algorithmic costs that are independent of dispatch

## Recommendation

Start with builtin member ids and a dedicated builtin-member opcode.

That is the smallest meaningful slice that attacks the current `CODE_RTMEMBER_IVK` bottleneck directly, reduces string-dispatch overhead, and gives a clean foundation for field-slot and user-method specialization afterward.
