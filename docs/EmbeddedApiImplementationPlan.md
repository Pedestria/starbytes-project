# Embedded API Implementation Plan

Last updated: March 25, 2026

## Goal

Finish and extend the Starbytes embedded API so host applications can:

- compile Starbytes source from memory
- execute compiled bytecode from memory
- capture diagnostics, runtime errors, output, and optional profiling data
- run in constrained hosts, including browser/WASM, without assuming filesystem or dynamic-library access

The target is not "make the CLI callable from another process". The target is a host-facing library surface with explicit ownership, predictable failure modes, and a browser-safe subset.

## Current State

The current scaffolding lives in:

- [`src/embed/include/starbytes.h`](/Users/alextopper/Documents/GitHub/starbytes-project/src/embed/include/starbytes.h)
- [`src/embed/starbytes.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/embed/starbytes.cpp)
- [`src/embed/CMakeLists.txt`](/Users/alextopper/Documents/GitHub/starbytes-project/src/embed/CMakeLists.txt)

What exists today:

- an initial `ByteBuffer` type
- an `ArcPtr` template
- placeholder `CompilationContext` / `ExecutionContext` types
- a partially started `compileModule(...)` implementation

What is missing or unsafe right now:

- `CompilationContext::compileModule(...)` is incomplete and returns nothing
- `ExecutionContext` entry points are declared but not implemented
- the compile path does not currently mirror the proven driver flow:
  - no `parser.finish()`
  - no `gen.setContext(...)`
  - no `gen.finish()`
  - no structured diagnostic capture
- the ownership layer is not safe enough for a public API:
  - `ArcPtr` keeps refcount by value instead of in shared control state
  - `ByteBuffer::~ByteBuffer()` uses `delete` for array storage
  - `ByteBuffer::copy(...)` allocates `bytes_n` but copies `size`
- the public surface is not embedding-friendly:
  - `std::ostream &` in ABI-facing structs
  - C++ template types in the public boundary
  - no stable error/result contract
  - no host callback surface for output, imports, or native bindings
- browser/WASM constraints are not represented:
  - runtime native loading is built around `dlopen` / `LoadLibrary`
  - compile and execute paths assume stream/file-oriented orchestration rather than explicit host-controlled buffers and callbacks

## Design Principles

The embedded API should follow these rules:

1. Reuse the real compiler/runtime, not a second implementation.
2. Make the stable boundary a C ABI with opaque handles and plain structs.
3. Keep an optional C++ wrapper for ergonomics, but do not make C++ ABI the portability boundary.
4. Prefer memory-first APIs over filesystem-first APIs.
5. Make host policy explicit:
   - output
   - diagnostics
   - script args
   - module resolution
   - native or host binding registration
6. Define a browser-safe subset instead of pretending the desktop/native surface will work unchanged in WASM.
7. Deliver a thin vertical slice first: single-module compile + execute from memory, then expand.

## Recommended Target Surface

The embedded API should split into two layers.

### Layer 1: Stable C ABI

Recommended public types:

- `StarbytesEmbedCompiler`
- `StarbytesEmbedRuntime`
- `StarbytesEmbedModule`
- `StarbytesEmbedBuffer`
- `StarbytesEmbedSource`
- `StarbytesEmbedDiagnostic`
- `StarbytesEmbedCompileOptions`
- `StarbytesEmbedCompileResult`
- `StarbytesEmbedExecuteOptions`
- `StarbytesEmbedExecuteResult`
- `StarbytesEmbedHost`

Recommended public capabilities:

- create/destroy compiler and runtime contexts
- compile in-memory sources into an owned module/bytecode buffer
- execute an owned module or raw bytecode buffer
- collect diagnostics and runtime errors in structured form
- capture stdout/stderr through callbacks or append-only host sinks
- optionally enable compile/runtime profiling
- optionally register host-provided native/builtin functions

Recommended boundary rules:

- no `std::ostream`, `std::string`, templates, or references in exported signatures
- use explicit `create/free` or `retain/release` functions for owned results
- use opaque handles for compiler/runtime/module internals
- return status codes plus result structs instead of implicit stream logging

### Layer 2: Thin C++ Wrapper

Keep a small C++ wrapper for internal or native host use:

- RAII around opaque C handles
- `std::string_view` and pointer-length buffer conveniences
- callback adapters for `std::function`

This wrapper should sit on top of the C ABI, not replace it.

## What The First Real Slice Should Support

The first complete end-to-end slice should be intentionally narrow:

- one logical module name
- one or more in-memory source files belonging to that module
- no filesystem module discovery
- no dynamic native-module loading
- compile diagnostics returned as strings/records
- execute from in-memory bytecode buffer
- runtime error returned as structured result
- stdout/stderr captured through host callbacks

That slice is enough to prove:

- desktop embedding
- unit/integration tests around the embed surface
- browser/WASM compilation and execution for pure Starbytes code

It is also the right thin slice because it avoids coupling the initial API to CLI-specific path discovery and native auto-loading behavior from [`tools/driver/main.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/main.cpp).

## Proposed Implementation Phases

### Phase 0: Reset The Public Boundary

Before finishing the current scaffold, fix the shape of the API.

Deliverables:

- move the public embed header to a stable include location such as:
  - `include/starbytes/embed.h`
  - optional `include/starbytes/embed.hpp`
- keep implementation in `src/embed`
- rename the shared library target to an embedding-specific target name if needed
- mark the current `src/embed/include/starbytes.h` scaffold as internal-only or replace it

Reasoning:

- `src/embed/include/starbytes.h` is currently both incomplete and the wrong ABI shape for browser/host integration
- stabilizing the wrong surface first will create rework

### Phase 1: Finish A Single-Module In-Memory Compile API

Implement compile orchestration by reusing the existing parser/codegen path:

- `Parser`
- `ModuleParseContext`
- `Gen`
- `ModuleGenContext`

Concrete work:

- extract or share the driver's proven compile orchestration instead of duplicating it ad hoc
- compile from `StarbytesEmbedSource[]` into an in-memory bytecode buffer
- capture diagnostics using the same `DiagnosticHandler` path the driver already uses
- expose compile options that are already real in the compiler:
  - module name
  - infer 64-bit numbers
  - profiling enabled
- return:
  - success/failure
  - owned bytecode buffer
  - diagnostics
  - optional compile profile snapshot

Success criteria:

- one regression test can compile a simple module entirely from memory
- invalid input returns deterministic diagnostics with no crash and no leaked ownership

### Phase 2: Finish A Single-Module In-Memory Execute API

Implement execution orchestration by reusing:

- `Runtime::Interp::Create()`
- `Interp::exec(std::istream &)`
- `hasRuntimeError()`
- `takeRuntimeError()`
- `getProfileData()`

Concrete work:

- execute directly from an in-memory bytecode buffer
- add host output callbacks instead of `std::ostream &`
- expose execution options:
  - script args
  - profiling enabled
  - host environment hooks
- return:
  - exit code / success code
  - runtime error string if present
  - captured output
  - optional runtime profile snapshot

Success criteria:

- one regression test compiles then executes a simple in-memory module
- runtime failures surface through the result object, not process-global logging alone

### Phase 3: Add Host Capabilities Explicitly

Once compile/execute round-trip works, add host-controlled integration points.

Required capabilities:

- output callback
- diagnostics callback or structured pull API
- script argument injection
- host module resolution callback for imports
- host binding registration for builtin/native functionality

This phase should separate two cases:

- desktop/native host:
  - can optionally load shared-library extensions if the host allows it
- browser/WASM host:
  - no `dlopen` / `LoadLibrary`
  - all allowed host functions must be pre-registered or linked in

Recommended rule:

- dynamic native loading is an optional host feature, not a baseline embed assumption

### Phase 4: Support Imports Without Recreating CLI Discovery Inside The API

The embed API should not hardcode the driver's filesystem resolver as its default contract.

Instead, define import handling in two layers:

- Level A:
  - caller supplies every source for one logical module
- Level B:
  - caller provides a resolver callback that returns source text for imported modules

This keeps the embedded API portable while still allowing:

- editor integration
- browser playgrounds
- server-side compilation services
- native hosts with custom package resolution

If a filesystem-backed resolver is useful, implement it as a host helper, not as the core embed contract.

### Phase 5: Browser/WASM Targeting

This phase makes the API truly usable "anywhere".

Concrete work:

- build the embed target to WASM
- define which stdlib/runtime features are unsupported or host-provided in browser builds
- replace direct native loading with a static host registry
- expose flat exported functions that JS can call without needing a C++ ABI
- ensure output/diagnostics move through callbacks or retrievable buffers, not process-global stdout assumptions

Important constraint:

- pure Starbytes code and explicitly host-provided capabilities should work in browser/WASM
- filesystem, process control, unrestricted networking, and dynamic shared-library loading are host-policy features and may be unavailable

### Phase 6: Hardening, Versioning, And Examples

Before calling the embedded API stable, add:

- API version query
- bytecode/module format version query
- ownership tests
- invalid-input fuzz coverage for compile and execute entry points
- browser example
- native host example
- documentation for supported and unsupported host capabilities

Also add CI coverage for:

- native embed smoke test
- WASM/browser smoke test if toolchain is available in CI

## Recommended Internal Refactors

The embed layer should not keep large amounts of orchestration logic unique to `src/embed`.

Recommended refactors:

- extract shared compile helpers from the driver into reusable library code
- extract shared runtime setup helpers where the same logic is needed by both driver and embed paths
- isolate host/environment state that is currently process-global when practical

Reasoning:

- the driver already contains the most correct compile/execute orchestration
- duplicating that flow in `src/embed` will drift and eventually produce two subtly different Starbytes runtimes

## Key Open Design Decisions

These decisions should be made explicitly before Phase 3 expands scope.

### 1. C ABI only, or C ABI plus C++ wrapper?

Recommendation:

- both, with C ABI as the real stability boundary

### 2. Should module imports be solved in phase 1?

Recommendation:

- no
- phase 1 should be single-module, memory-only
- imports should arrive after the basic round-trip is proven

### 3. Should dynamic native loading be part of the browser story?

Recommendation:

- no
- browser/WASM should use host registration, not shared-library loading

### 4. Should diagnostics be streamed or returned?

Recommendation:

- support both eventually
- start with stored structured diagnostics in the result object
- add streaming callbacks later if needed

## Thin-Slice Delivery Order

The highest-value order is:

1. replace the current public scaffold with a stable embed boundary
2. ship compile-from-memory for a single logical module
3. ship execute-from-memory for that compiled module
4. add output/error/profile capture
5. add host resolver and host binding hooks
6. add browser/WASM build and example

That order proves the embedding model early and avoids getting trapped in filesystem/native-module edge cases before the core API works.

## Immediate Next Step

The first implementation task should be:

- design and land the new public embed header and result/ownership types

Then immediately build the smallest end-to-end test:

- compile one in-memory source string
- execute the resulting bytecode from memory
- assert on output and runtime error state

If that slice is solid, the rest of the embedded API can expand without guessing.
