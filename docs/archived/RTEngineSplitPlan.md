# RTEngine Split Plan

## Goal

Break `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp` into smaller, readable implementation units without changing runtime behavior.

This is a manageability refactor first, not a runtime redesign.

## Assumptions

- The public runtime API remains stable in `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/runtime/RTEngine.h`.
- `InterpImpl` remains an internal runtime type.
- The split should be mechanical and low-risk.
- Runtime behavior, bytecode semantics, and performance characteristics should remain unchanged by the refactor itself.

## Current State

`/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp` is about 4745 lines and currently mixes several distinct concerns:

1. Low-level runtime helpers
   - UTF-8 helpers
   - numeric promotion/conversion helpers
   - attribute/native name resolution helpers
   - parsing/string utility helpers
2. Storage and runtime state
   - `RTScope`
   - `_StarbytesFuncArgs`
   - `RTAllocator`
   - local frame and slot helpers
3. Invocation and runtime services
   - native invocation
   - function invocation
   - microtask processing
   - class hierarchy helpers
4. Expression execution
   - `evalExpr`
   - quickening inspection/execution
   - expression skipping
5. Statement and block execution
   - `execNorm`
   - runtime block execution
   - top-level `exec`
   - extension loading

## Recommended Target Structure

### Public header remains unchanged

Keep:
- `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/runtime/RTEngine.h`

This should continue to expose only the `Interp` interface and profile data.

### Add a private internal header

Add:
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineInternal.h`

This internal header should contain:
- `RTScope`
- `_StarbytesFuncArgs`
- `RTAllocator`
- full `InterpImpl` declaration
- shared private helper declarations needed across the new `.cpp` files

This keeps `InterpImpl` private while allowing a clean multi-file implementation.

### Split implementation into focused files

#### 1. Support helpers

Add:
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineSupport.cpp`

Contents:
- UTF-8 helper functions
- numeric type promotion and conversion helpers
- attribute/native symbol name resolution helpers
- other file-local pure helpers

This file should hold logic that is stateless or close to stateless.

#### 2. Allocator, locals, and quickening helpers

Add:
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineLocals.cpp`

Contents:
- `RTAllocator`
- local frame handling
- local slot load/store helpers
- local-slot numeric inspection helpers
- quickening candidate inspection helpers

This is a coherent subsystem and should remain grouped.

#### 3. Invocation and runtime services

Add:
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineInvoke.cpp`

Contents:
- native function invocation
- runtime function invocation
- invocation-with-values helpers
- microtask processing
- class hierarchy building
- extension loading if that remains the best fit

This isolates call-path behavior from opcode execution.

#### 4. Expression and statement execution

Add:
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineExec.cpp`

Contents:
- `evalExpr`
- skip helpers
- runtime block execution
- `execNorm`
- top-level `exec`

This keeps opcode execution and statement/block dispatch together, which is where most future runtime work will likely continue.

### Thin wrapper translation unit

Keep a reduced:
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp`

This should contain only:
- factory creation glue such as `Interp::Create()`
- any truly top-level runtime glue that does not fit elsewhere

## Why This Shape

- It follows the file’s existing seams rather than inventing new abstractions prematurely.
- It does not expose `InterpImpl` outside the runtime implementation.
- It keeps opcode execution close together, which is important for future runtime optimization work.
- It keeps local-slot and quickening logic together, which is already a coherent internal area.
- It avoids mixing runtime refactoring with behavior changes.

## Implementation Order

1. Add `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineInternal.h`
   - lift private structs, allocator, and `InterpImpl` declaration into it
2. Move helper functions into `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineSupport.cpp`
3. Move allocator/local-slot/quickening helpers into `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineLocals.cpp`
4. Move native/function invocation, microtasks, and hierarchy helpers into `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineInvoke.cpp`
5. Move `evalExpr`, skip logic, block execution, and `exec` into `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngineExec.cpp`
6. Update the runtime build configuration to compile the new translation units
7. Rebuild `starbytes`
8. Run the full test suite

## Risk Points

- Helper visibility:
  - many current `static` helpers will need either to remain translation-unit local or be declared in the internal header
- `InterpImpl` coupling:
  - method declarations and member layout must remain consistent across all translation units
- Execution coupling:
  - `evalExpr`, `execNorm`, and skip helpers are tightly related and should not be fragmented too aggressively
- Refactor discipline:
  - no runtime behavior changes should be mixed into the split

## Validation Strategy

Minimum validation after the split:
- rebuild `/Users/alextopper/Documents/GitHub/starbytes-project/build/bin/starbytes`
- run the full suite
- re-run the Track A benchmark kernels that heavily exercise runtime execution:
  - `binary-trees`
  - `n-body`
  - `spectral-norm`

The benchmark re-run is not to require identical timing, but to catch accidental path regressions or obvious runtime breakage.

## Thin-Slice Alternative

If a lower-risk first pass is preferred, do the split in two waves:

### Wave 1
- add `RTEngineInternal.h`
- extract `RTEngineSupport.cpp`
- extract `RTEngineLocals.cpp`

### Wave 2
- extract `RTEngineInvoke.cpp`
- extract `RTEngineExec.cpp`
- shrink `RTEngine.cpp` to thin glue

This is safer but leaves the main file large after the first pass.

## Recommendation

Proceed with the full four-file split in one pass, but keep it strictly mechanical.

That gives a meaningful readability win immediately while preserving a clean internal architecture for later runtime work.
