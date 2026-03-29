# Member/Builtin Dispatch Plan

## Goal

Track the remaining Starbytes member-dispatch work after the direct-call runtime changes and the now-implemented phase-2 member/builtin fast paths.

This document now serves two purposes:

- record the original member/builtin dispatch plan
- track what has landed in the runtime/compiler as of March 29, 2026

The target remains lower interpreter cost for:

- builtin string/array/dict/regex member calls
- class field reads and writes
- follow-on user-method specialization where Track A still shows generic member dispatch

## Status Update

### Implemented

The current runtime/compiler now includes these member-dispatch features:

- builtin member id enum in runtime bytecode metadata:
  - [`include/starbytes/compiler/RTCode.h:143`](/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/compiler/RTCode.h#L143)
- dedicated builtin-member call opcode:
  - [`include/starbytes/compiler/RTCode.h:65`](/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/compiler/RTCode.h#L65)
- codegen-side builtin member recognition for instance calls:
  - [`src/compiler/CodeGen.cpp:1996`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L1996)
- runtime builtin-member dispatch by receiver kind plus builtin id:
  - [`src/runtime/RTEngine.cpp:1107`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L1107)
  - [`src/runtime/RTEngine.cpp:3122`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3122)
- direct field-slot get/set opcodes:
  - [`include/starbytes/compiler/RTCode.h:66`](/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/compiler/RTCode.h#L66)
  - [`include/starbytes/compiler/RTCode.h:67`](/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/compiler/RTCode.h#L67)
- codegen-side direct class field-slot emission when semantic info can prove the slot:
  - slot inference: [`src/compiler/CodeGen.cpp:891`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L891)
  - member get emission: [`src/compiler/CodeGen.cpp:1717`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L1717)
  - member set emission: [`src/compiler/CodeGen.cpp:1920`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L1920)
- runtime direct field-slot load/store path:
  - [`src/runtime/RTEngine.cpp:4296`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4296)
  - [`src/runtime/RTEngine.cpp:4370`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4370)
- tooling support for the new opcodes in skip/disasm/profile paths:
  - [`src/compiler/RTCode.cpp:360`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/RTCode.cpp#L360)
  - [`src/runtime/RTDisasm.cpp:170`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTDisasm.cpp#L170)
  - [`tools/driver/profile/RuntimeProfile.cpp:63`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/profile/RuntimeProfile.cpp#L63)

### Partially implemented

- builtin member calls are now specialized, but metadata members like `.length` still use the generic member-get path
- direct field-slot get/set exists, but only for sites where codegen can recover the receiver class layout from available semantic info
- builtin member codegen currently recognizes by member name and leaves final receiver-kind validation to runtime

### Not yet implemented

- dedicated builtin metadata get opcode
- monomorphic user-method call cache
- quickening from generic member invoke to specialized member call shapes
- broader direct method-slot caching for user-defined class methods

## Current Runtime Shape

### Codegen shape

Codegen no longer treats all instance member calls the same.

Current behavior:

- builtin-named instance calls emit `CODE_RTCALL_BUILTIN_MEMBER`
- proven class field reads emit `CODE_RTMEMBER_GET_FIELD_SLOT`
- proven class field writes emit `CODE_RTMEMBER_SET_FIELD_SLOT`
- unresolved or dynamic cases still fall back to `CODE_RTMEMBER_GET`, `CODE_RTMEMBER_SET`, and `CODE_RTMEMBER_IVK`

Relevant code:

- builtin member emission: [`src/compiler/CodeGen.cpp:1996`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L1996)
- direct field get emission: [`src/compiler/CodeGen.cpp:1717`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L1717)
- direct field set emission: [`src/compiler/CodeGen.cpp:1920`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L1920)

### Runtime builtin-member path

Builtin member invocation is now split out of the generic member opcode:

1. evaluate receiver
2. decode builtin member id
3. read args into the inline-capable `DirectArgBuffer`
4. switch on receiver runtime kind
5. execute the builtin handler
6. fall back to normal class-method resolution only when the receiver is not a builtin container/string/regex

Relevant code:

- builtin dispatcher: [`src/runtime/RTEngine.cpp:1107`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L1107)
- opcode dispatch: [`src/runtime/RTEngine.cpp:3122`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3122)

### Runtime field-slot path

Direct field-slot access now bypasses member-name string lookup entirely when the slot is encoded in bytecode.

Relevant code:

- slot get: [`src/runtime/RTEngine.cpp:4296`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4296)
- slot set: [`src/runtime/RTEngine.cpp:4370`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4370)
- object/layout guard: [`src/runtime/RTEngine.cpp:1042`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L1042)

### Remaining generic paths

The following still go through broader dispatch than we ultimately want:

- metadata reads like `.length`
- user-defined method calls via `CODE_RTMEMBER_IVK`
- any field access where codegen cannot prove the receiver class slot safely

Relevant code:

- generic member get: [`src/runtime/RTEngine.cpp:4267`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4267)
- generic member set: [`src/runtime/RTEngine.cpp:4324`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4324)
- generic member invoke: [`src/runtime/RTEngine.cpp:4392`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4392)

## Current Runtime Signal

The motivating profile signal is still the same one that led to this phase:

- `binary-trees` remained field-access heavy
- `k-nucleotide` remained builtin/member heavy

The latest pre-phase-2 profile evidence referenced by the proposal is still:

- [`benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_binary-trees.runtime-profile.json`](/Users/alextopper/Documents/GitHub/starbytes-project/benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_binary-trees.runtime-profile.json)
- [`benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_k-nucleotide.runtime-profile.json`](/Users/alextopper/Documents/GitHub/starbytes-project/benchmark/results/raw/track_a_steady-state_20260325T230531Z/steady-state_k-nucleotide.runtime-profile.json)

Interpretation:

- phase 2 was the right next slice
- the next benchmark run should specifically confirm whether builtin-heavy and field-heavy workloads moved from the new opcodes into lower `member_access` time

## Proposal Status By Item

## 1. Separate member access into explicit bytecode families

### Status

Partially implemented.

What landed:

- `CODE_RTCALL_BUILTIN_MEMBER`
- `CODE_RTMEMBER_GET_FIELD_SLOT`
- `CODE_RTMEMBER_SET_FIELD_SLOT`

What remains:

- no dedicated builtin metadata get opcode
- no direct user-method opcode
- generic dynamic member read/write/invoke still exists as the fallback path

## 2. Add builtin member ids

### Status

Implemented for the current runtime builtin-member fast path.

What landed:

- bytecode enum ids for current builtin member coverage
- runtime helper mapping from member name to builtin id
- runtime helper mapping from builtin id back to member name for fallback/tooling

Relevant code:

- [`include/starbytes/compiler/RTCode.h:143`](/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/compiler/RTCode.h#L143)
- [`src/compiler/RTCode.cpp:12`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/RTCode.cpp#L12)

## 3. Add codegen-side builtin member recognition

### Status

Implemented.

What landed:

- instance calls with builtin-recognized member names emit `CODE_RTCALL_BUILTIN_MEMBER`
- this does not require static receiver type proof; runtime still validates the receiver kind

Relevant code:

- [`src/compiler/CodeGen.cpp:1996`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/CodeGen.cpp#L1996)

## 4. Dispatch builtin members by receiver-kind switch plus enum id

### Status

Implemented.

What landed:

- receiver-kind split for String, Regex, Array, and Dict
- `DirectArgBuffer` reuse instead of the old `std::vector<StarbytesObject>` shape for specialized builtin calls

Relevant code:

- [`src/runtime/RTEngine.cpp:1107`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L1107)

## 5. Add direct field slot opcodes for class fields

### Status

Implemented for proven class field sites.

What landed:

- semantic class-field slot recovery in codegen
- slot-based read/write bytecode emission
- runtime slot-based field load/store guarded by object layout compatibility

What remains:

- broader field-slot coverage for cases where semantic type recovery is still too weak

## 6. Add monomorphic method caches

### Status

Not implemented.

This is now the clearest next member-dispatch slice after phase 2.

## Validation Strategy

After each new member-dispatch slice:

1. build `starbytes`
2. capture runtime profiles for:
   - `binary-trees`
   - `k-nucleotide`
   - `fasta`
3. compare against the previous profile with `tools/driver/profile/compare_runtime_profiles.py`
4. run Track A

Primary metrics:

- total runtime
- `member_access` subsystem time
- opcode counts for:
  - `CODE_RTMEMBER_GET`
  - `CODE_RTMEMBER_SET`
  - `CODE_RTMEMBER_IVK`
  - `CODE_RTCALL_BUILTIN_MEMBER`
  - `CODE_RTMEMBER_GET_FIELD_SLOT`
  - `CODE_RTMEMBER_SET_FIELD_SLOT`

## Recommended Next Order

Implement the remaining work in this order:

1. rerun Track A and runtime profiles on the current phase-2 implementation
2. add dedicated builtin metadata get specialization if `.length` still matters materially
3. add monomorphic user-method call caching
4. consider call-site quickening from generic member invoke to specialized member call forms

Reasoning:

- the new phase-2 work should be measured before broadening the design again
- builtin metadata reads and user methods are the main remaining generic member paths
- the current implementation already captures the lower-risk builtin-call and direct-field wins

## Non-goals

- no full dynamic inline cache framework yet
- no broad property-bag redesign
- no attempt to make arbitrary dynamic properties as cheap as proven class fields
- no attempt to solve algorithmic collection costs unrelated to dispatch
