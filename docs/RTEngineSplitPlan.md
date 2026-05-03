# RTEngine Split Plan

## Purpose

`src/runtime/RTEngine.cpp` is now ~7,950 lines and `InterpImpl` carries ~71 member functions plus inner type definitions for V2 image, local frame, feedback sites, and class layout. Reading or modifying any subsystem (member dispatch, V2 image build, expression evaluation, JIT integration) requires scrolling past unrelated code.

This document proposes a phased split into smaller, topical translation units. The goal is **readability and reviewability**, not performance, not new abstractions.

## Non-Goals

- No change to runtime semantics, no API change, no opcode change.
- No new build-time dependencies, no header-only churn that hides cost in include graphs.
- No change to public `Interp` interface.
- No "interface-everywhere" rewrite. Subsystems stay tightly coupled to the interpreter — the split is a file-layout refactor, not an architecture refactor.

## Coupling Constraint And Pattern Choice

Almost every helper in `InterpImpl` reads or writes one or more of:

- `lastRuntimeError`
- `runtimeProfile` / `runtimeProfilingEnabled`
- `allocator`
- `localFrames`
- `classes`, `classTypeByName`, `classIndexByType`, `classLayouts`, `runtimeClassRegistry`
- `functions`, `functionIndexByName`
- the active `RTCode` opcode stream
- the various feedback-site maps
- `codeCache` (Phase 4)

Splitting through pure interfaces would mean defining an interface for almost every field, then plumbing them through constructors. That cost is not paid back by readability.

**Pattern choice**: each subsystem is a class that holds a back-pointer (`InterpImpl *`) and is declared `friend class InterpImpl;` (or vice-versa). Subsystem methods reach into the core fields directly, exactly as today, but the *file* they live in is now topical. Where a subsystem owns its own state (e.g. the V2 execution image cache, the code cache, the class registry), that state migrates into the subsystem class.

This keeps the diff mechanical, keeps semantics identical, and gives reviewers a real per-file home for each concern.

## Test Discipline Per Phase

Every phase must, before merge:

1. Build clean with the existing CMake `file(GLOB)` sweep — no manifest edits unless adding a new test.
2. Pass `ctest` excluding the three pre-existing failures (`parse-test`, `compile-test`, `run-test` — all caused by `tests/test.starb` referencing stdlib symbols that aren't loaded in the test fixture). These pre-existed and are unrelated.
3. Pass the Phase 1–4 Execution Model tests in particular: `bytecode-v2-phase1-test`, `feedback-vectors-phase2-test`, `execution-model-phase3-test`, `execution-model-phase4-test`. These are the most direct guards against runtime regressions.
4. Run one steady-state benchmark (e.g. `n-body` or `spectral-norm`) and confirm runtime is within ±5% of the previous phase's baseline. The split should not move performance.

If a phase introduces a regression, **revert that phase** and rescope. Do not stack subsequent phases onto a regressed base.

## Phase Map

| Phase | Extracts | New files | Lines moved (approx) | Risk |
|------:|----------|-----------|---------------------:|-----:|
| 0 | Free-function utilities | `RTValue`, `RTStream`, `RTNumeric` | ~700 | low |
| 1 | `RTAllocator` | `RTAllocator.{h,cpp}` | ~145 | low |
| 2 | Local frame + slot helpers | `RTLocalFrame.{h,cpp}` | ~400 | low–med |
| 3 | Class registry + layout | `RTClassRegistry.{h,cpp}` | ~250 | low |
| 4 | V2 image build + loop discovery + Tier 2 IR | `RTImage.{h,cpp}` | ~700 | medium |
| 5 | Member / builtin dispatch | `RTMemberDispatch.{h,cpp}` | ~700 | medium |
| 6 | Adaptive quickening | `RTQuickening.{h,cpp}` | ~600 | low |
| 7 | Expression evaluator (`evalExpr`) | `RTExpression.{h,cpp}` | ~2,300 | high |
| 8 | Statement dispatch (`execNorm`, blocks) | `RTStatement.{h,cpp}` | ~500 | medium |
| 9 | Function call + microtasks + V2 dispatch loop | `RTFunction.{h,cpp}`, `RTV2Dispatch.{h,cpp}` | ~1,400 | high |
| 10 | Interpreter core (entry, lifecycle) | `RTInterpreter.{h,cpp}` (rename from `RTEngine.cpp`) | residual | low |

Total moved: ~7,700 of the current ~7,950 lines. The renamed `RTInterpreter.cpp` should end at a few hundred lines holding only `InterpImpl` lifecycle, `Interp::Create`, `exec`, `addExtension`, and the interpreter's persistent state.

---

## Phase 0 — Pure Helpers Out

**Move**

- `MemoryStreamBuf`, `MemoryInputStream` (lines ~209–260) → `RTStream.{h,cpp}` (already heavy enough for a private header).
- `DirectArgBuffer` (~262–428) → `RTStream.cpp` or its own `RTArgBuffer.{h,cpp}`. Used by the call ABI; will be referenced from Phase 9.
- Numeric helpers `isIntegralNumType` / `isFloatingNumType` / `numericTypeRank` / `promoteNumericType` / `objectToNumber` / `makeNumber` / `typedKindFromNumType` / `numTypeFromTypedKind` / `extractTypedNumericValue` / `computeTypedBinaryResult` (~429–615) → `RTNumeric.{h,cpp}`.
- `runtimeObjectEquals`, `isDictKeyObject`, `clampSliceBound`, `parseIntStrict`, `parseFloatStrict`, `parseDoubleStrict` (~617–710) → `RTValue.{h,cpp}`.
- `isExpressionOpcode`, `siteKindForOpcode` (~109–205) → `RTOpcodeMeta.{h,cpp}`.

**Why this first.** Zero state, zero coupling to `InterpImpl`. Pure mechanical move. Lets later phases include topical headers instead of dragging `RTEngine.cpp`'s internals.

**Exit criteria**

- `RTEngine.cpp` no longer defines any free function above the `RTAllocator` declaration.
- New headers are private (`src/runtime/` or `include/starbytes/runtime/internal/` — pick one and stick to it; the existing pattern in `src/runtime/RTStdlib.h` suggests `src/runtime/`).
- All tests green.

---

## Phase 1 — `RTAllocator`

**Move**

- `RTAllocator` class (~712–855) → `RTAllocator.{h,cpp}`.
- `ScheduledTaskCall` (~855–860) goes with the call/microtask phase later; for Phase 1, keep it in the residual file.

**Pattern**

- Self-contained. No back-pointer needed. Pure data class with map-of-maps storage and scope generation tracking.

**Risk** — very low. `RTAllocator` is already a well-bounded class.

**Exit criteria**

- `RTEngine.cpp` no longer holds the class definition.
- `InterpImpl` still owns `std::unique_ptr<RTAllocator> allocator;` exactly as today.

---

## Phase 2 — `RTLocalFrame`

**Move**

- `LocalSlot`, `LocalFrame` structs (~918–931) → `RTLocalFrame.h`.
- Methods: `pushLocalFrame`, `popLocalFrame`, `currentLocalFrame` (×2), `referenceLocalSlot`, `storeLocalNumericValue`, `copyLocalSlotValue`, `localSlotIsTruthy`, `localSlotToNumber`, `localSlotToIndex`, `observedLocalSlotNumericType`, `storeLocalSlotOwned`, `storeLocalSlotBorrowed` (~2389–2761) → `RTLocalFrame.cpp`.

**Pattern**

- New class `LocalFrameStack` owns `std::vector<LocalFrame> localFrames` and `std::vector<LocalFrame> localFrameFreeList`.
- Holds back-pointer to `InterpImpl` (for `lastRuntimeError`, `runtimeProfile`, allocator interactions in `popLocalFrame` if any).
- `InterpImpl` exposes `LocalFrameStack &frames();` and routes existing call sites through it. Existing methods become thin forwarders during a transition window, then call sites get rewritten phase-by-phase to call into `frames().localSlotToNumber(...)` directly. To keep the diff small in Phase 2, leave the forwarders in place; remove them in Phase 10 cleanup.

**Risk** — low–medium. Slot helpers are called from many places; watch for any `friend`-only access.

**Exit criteria**

- `LocalFrame` and `LocalSlot` no longer defined inside `InterpImpl`.
- `localFrames` and `localFrameFreeList` migrated into `LocalFrameStack`.
- Phase 1–4 execution-model tests still green.

---

## Phase 3 — `RTClassRegistry`

**Move**

- `RuntimeClassLayout` (~869–873) → `RTClassRegistry.h`.
- Fields: `classes`, `classTypeByName`, `classIndexByType`, `classLayouts`, `runtimeClassRegistry`.
- Methods: `findClassByName`, `findClassByType`, `buildClassHierarchy`, `findMethodOwnerInHierarchy`, `findClassLayout`, `rebuildClassLayout`, `classifyReceiverKind`, `lookupClassFieldSlot`, `canAccessDirectFieldSlot`, `resolveClassMethod` (~1248–1424) → `RTClassRegistry.cpp`.

**Pattern**

- `ClassRegistry` class with the four containers plus the layout cache. Holds back-pointer to `InterpImpl` only for error reporting (`lastRuntimeError`).
- `CachedReceiverKind` enum stays public on the registry.

**Risk** — low. The class registry is read-mostly from the rest of the runtime; writes happen at module load and at custom-class allocation. No tight loops.

**Exit criteria**

- `InterpImpl` exposes `ClassRegistry &classRegistry();`.
- All class/layout/dispatch-resolution call sites route through it.
- `feedback-vectors-phase2-test` (which exercises class layout caches) still green.

---

## Phase 4 — `RTImage` (V2 execution image)

**Move**

- `V2ExecutionImage`, `V2LoopRuntimeState`, `V2ExecOpcode`, `V2ExecInstruction` (~1013–1102) → `RTImage.h`.
- Methods: `getOrBuildV2ExecutionImage`, `buildV2ExecutionImage`, `discoverV2Loops`, `noteLoopGuardObservation`, `maybeTriggerHotLoopTier2`, `buildTier2LoopIR` (~2762–3189) → `RTImage.cpp`.
- The map `v2ExecutionImages` migrates into the image owner.

**Pattern**

- `V2ImageBuilder` class owns the per-function image cache. Compilation hand-off (`Tier2JitCompiler::compile` + `codeCache.allocateOptimized`) stays inside `maybeTriggerHotLoopTier2`, so the image owner has access to `InterpImpl` (or specifically to `codeCache` and `runtimeProfile`) via back-pointer.
- The Tier 2 IR types in `include/starbytes/runtime/RTJit.h` (added in Phase 4 of the execution-model plan) stay where they are — they are already public to the Runtime namespace.

**Risk** — medium. Touches the Phase 4 JIT integration path. The OSR hook in the V2 dispatch loop must be moved or preserved when `invokeV2FuncWithValues` migrates in Phase 9; sequence carefully.

**Exit criteria**

- `RTEngine.cpp` no longer defines `V2ExecOpcode` or `V2ExecutionImage`.
- The OSR check at the loop header still triggers compiled execution.
- `execution-model-phase3-test` and `execution-model-phase4-test` both green.
- Benchmark within ±5%.

---

## Phase 5 — `RTMemberDispatch`

**Move**

- `invokeResolvedClassMethod` (~1426–1448).
- `invokeBuiltinMember` (~1450–~2167) — by far the largest single member function in this region; the bulk of the move.
- The feedback-site structs (`MemberGetFeedbackSite`, `MemberSetFeedbackSite`, `MemberInvokeFeedbackSite`, `VarRefFeedbackSite`) (~984–1011) and their maps (~1130–1133) also belong here.
- The associated cache helpers (`buildFeedbackKey`, `recordFeedbackSiteInstall`, `recordFeedbackCacheHit`, `recordFeedbackCacheMiss`) (~2294–2333) move with them.

**Pattern**

- `MemberDispatch` class owns the four feedback maps and provides `invokeMember`, `getMember`, `setMember`, `invokeBuiltin`. Reaches back into `InterpImpl` for the function-call entry point and for class-registry queries.

**Risk** — medium. `invokeBuiltinMember` is huge (~700 lines) and central to string/array/dict performance. Keep the move purely mechanical: same function, same body, new file.

**Exit criteria**

- `feedback-vectors-phase2-test` green.
- Builtin-heavy benchmarks (`fasta`, `k-nucleotide`) within ±5%.

---

## Phase 6 — `RTQuickening`

**Move**

- `RTQuickenedExpr` and related types (defined in `RTCode.h` — those stay).
- `inspectLocalRefExpr`, `inspectQuickeningCandidate`, `getOrInstallQuickenedExpr`, `tryExecuteQuickenedExpr` (~3497–3970) → `RTQuickening.{h,cpp}`.

**Pattern**

- `Quickener` class with a back-pointer for slot/numeric access. Owns no extra state — quickened expressions are stored on the bytecode position via the runtime-profile machinery (already external).

**Risk** — low. The quickening pass is well-contained and exercised by `adaptive-quickening-phase5-test`.

**Exit criteria**

- `adaptive-quickening-phase5-test` green.

---

## Phase 7 — `RTExpression`

**Move**

- `evalExpr` (~4788–7100, **~2,300 lines**) → `RTExpression.cpp`.
- `skipExpr`, `skipExprFromCode`, `discardExprArgs` (~7102–7390 minus what stays with statements) → same file.

**Pattern**

- New class `ExpressionEvaluator` with back-pointer to `InterpImpl`. All field accesses route through the back-pointer with no field migration — the evaluator borrows everything.
- This phase is the **single biggest readability win**. After this phase, `RTEngine.cpp` is roughly half its current size.

**Risk** — **high**. `evalExpr` reaches into virtually every subsystem. The move itself is mechanical (one giant cut + paste + rewrap as `ExpressionEvaluator::eval`), but verifying it didn't subtly change any control flow needs the full test sweep plus benchmarks.

**Strategy to lower risk**

1. Do not refactor inside `evalExpr` during this phase. Move bytes only.
2. Land the move behind no flag — it should be invisible to behavior.
3. Run **all** non-pre-existing-failing tests, plus all Track A benchmarks at default sample count.
4. If anything moves outside ±5% on a benchmark, revert.

**Exit criteria**

- `RTEngine.cpp` no longer contains `evalExpr`.
- `RTExpression.cpp` is under 2,400 lines (one giant function plus its skip helpers; subsequent further-split is a separate exercise).
- Full test green.
- Track A benchmarks within ±5%.

---

## Phase 8 — `RTStatement`

**Move**

- `execNorm` (~7532–7822) → `RTStatement.cpp`.
- `executeRuntimeBlock`, `skipRuntimeStmt`, `skipRuntimeBlock` (~7391–7531) → same file.

**Pattern**

- `StatementDispatcher` class with back-pointer. The statement loop and the block-skipping helpers move together because they share the opcode space and the willReturn / return-value plumbing.

**Risk** — medium. `execNorm` is the V1 statement dispatch loop; bytecode-version-compatibility-test exercises it.

**Exit criteria**

- `bytecode-version-compatibility-test` green.

---

## Phase 9 — `RTFunction` and `RTV2Dispatch`

**Move**

- Function-template lifecycle and lookup: `registerFunctionTemplate`, `findFunctionByName` (~2169–2200) → `RTFunction.cpp`.
- Native-call dispatch: `invokeNativeFunc`, `invokeNativeFuncWithValues` (~4013–4099) → `RTFunction.cpp`.
- V1 fallback: `executeV1FallbackBlob` (~4101–4115).
- The generic call entry: `invokeFunc`, `invokeFuncWithValues` (~4561–4700, 4774–4787) → `RTFunction.cpp`.
- Microtasks: `processOneMicrotask`, `processMicrotasks`, `scheduleLazyCall`, `microtaskQueue`, `isDrainingMicrotasks`, `ScheduledTaskCall` → `RTFunction.cpp`.
- **Separately**: V2 dispatch loop `invokeV2FuncWithValues` (~4117–4559) → `RTV2Dispatch.cpp`. This is its own file because it's the hottest function in the runtime, the one Phase 4 hooked OSR into, and the most likely target for future micro-optimization. It deserves not sharing a file with the generic call entry.

**Pattern**

- `FunctionDispatcher` class for the cross-cutting call entry.
- `V2Dispatcher` class for the V2 register-VM loop. Holds back-pointer to `InterpImpl` for everything (frames, image, code cache, profile).
- The Phase 4 JIT OSR hook stays inside `V2Dispatcher::run`.

**Risk** — high. `invokeV2FuncWithValues` is hot and was just modified for Phase 4. Mechanical move only. Keep the OSR check at the same logical position.

**Exit criteria**

- `execution-model-phase4-test` green (this is the strictest guard).
- All Track A benchmarks within ±5%.
- Microtask-using tests green (whichever fixtures cover async scheduling).

---

## Phase 10 — `RTInterpreter`

**Rename and trim**

- `RTEngine.cpp` → `RTInterpreter.cpp`.
- Holds: `InterpImpl` constructor, destructor, `exec`, `addExtension`, the public `Interp::Create`, the `RuntimeFeedbackKey`/hash plumbing if not yet moved, and the residual cross-subsystem fields (`lastRuntimeError`, `runtimeProfile`, `runtimeProfilingEnabled`, `executionMode`, `jitEnabled`, `lastRuntimeError`, `activeModuleHeader`, `activeModuleCodeStart`).
- All subsystem objects (`ClassRegistry`, `LocalFrameStack`, `MemberDispatch`, `Quickener`, `V2ImageBuilder`, `ExpressionEvaluator`, `StatementDispatcher`, `FunctionDispatcher`, `V2Dispatcher`, `CodeCache`) are members of `InterpImpl`.

**Cleanup pass**

- Remove the temporary forwarder methods on `InterpImpl` introduced in Phases 2/3 that simply delegate to a subsystem; rewrite call sites to call subsystem methods directly. This is the only phase where call sites churn beyond pure file moves.
- Update `include/starbytes/runtime/RTEngine.h` only if the public `Interp` interface changes (it should not).
- Optionally rename the public header to `RTInterpreter.h` — but keep `RTEngine.h` as a deprecated forward-include for one release to avoid breaking anything that includes it externally.

**Risk** — low if everything before it landed cleanly.

**Exit criteria**

- `RTInterpreter.cpp` is under ~600 lines.
- No file in `src/runtime/` exceeds ~1,800 lines.
- All non-pre-existing-failing tests green.
- Track A benchmarks within ±5% of pre-Phase-0 baseline.

---

## Build System Notes

- `CMakeLists.txt` already does `file(GLOB STARBYTES_RUNTIME_SRCS ${CMAKE_SOURCE_DIR}/src/runtime/*.cpp ${CMAKE_SOURCE_DIR}/src/runtime/*.c)`. New `.cpp` files in `src/runtime/` are picked up automatically. No CMake edits required for any phase except possibly Phase 10 if rename uses `RTEngine.cpp` → `RTInterpreter.cpp` (the glob handles that too, but the install rules need a quick scan).
- Private headers can live in `src/runtime/` (matching `RTStdlib.h`) or `include/starbytes/runtime/internal/`. **Pick one and stick to it.** Current code uses `src/runtime/RTStdlib.h`. Recommendation: keep new internal headers there to match precedent; only put a header under `include/starbytes/runtime/` if it is part of the public Runtime API (as `RTJit.h` and `RTCodeCache.h` already are).

## Sequencing And Parallelism

The phases above are listed in dependency order. Most can be sequenced strictly. A few can be parallelized if more than one engineer is working:

- Phase 0 must be first.
- Phases 1, 3, 6 are independent of each other and could be parallelized after Phase 0.
- Phase 2 should land before Phase 7 (frame helpers are heavily used in `evalExpr`).
- Phase 4 should land before Phase 9 (the V2 dispatcher knows about the image owner).
- Phase 7 should land before Phase 8 (statement dispatch calls into expression evaluation).
- Phase 9 must be last before Phase 10.

A reasonable single-track order is **0 → 1 → 2 → 3 → 6 → 4 → 5 → 7 → 8 → 9 → 10**.

## What This Plan Is Not

- It is not an opportunity to redesign subsystems. Each phase moves bytes and adds the minimum class scaffolding to make the move legal. Real refactoring (e.g. simplifying `evalExpr` into per-opcode handlers, extracting the V2 dispatch's superinstruction handling, splitting `invokeBuiltinMember` per receiver kind) is left for follow-up work that builds on the cleaner per-file boundaries this plan establishes.
- It is not an excuse to introduce dependency injection, virtual dispatch, or interface contracts between subsystems. Friend access through a back-pointer is the entire architectural shift being proposed.
- It does not change the public `Interp` API or the on-disk bytecode format.

## Rollback

Any phase that fails its exit criteria should be reverted as a unit. Because the moves are mechanical and the per-phase commits are scoped, `git revert <phase-commit>` is sufficient. Subsequent phases assume the prior phase landed cleanly; do not stack a phase on a known-broken base.
