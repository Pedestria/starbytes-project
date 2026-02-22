# Stdlib Expansion Roadmap

Last updated: February 21, 2026

## Purpose
Define a pragmatic path to make Starbytes a complete toolchain stdlib without duplicating already-shipped module and builtin surfaces.

## Existing Baseline (Already Implemented)
- Modules: `IO`, `FS`, `CmdLine`, `Time`, `Threading`, `Unicode`.
- Builtins: `String`, `Array`, `Dict`, `Map`, `Regex`, `Task`, numerics, `Any`, `Bool`, `Void`.
- Important note: `Regex` is already a builtin type/literal surface. New roadmap phases should not introduce a duplicate standalone regex module.

## Duplication Guardrails
- Keep stream/file read-write APIs in `IO`.
- Keep path + metadata operations in `FS`.
- Keep runtime argument parsing in `CmdLine`.
- Keep regex as builtin; only add optional text-level helpers where justified.

## Phased Plan

### Phase 1 (Now): Runtime and Tooling Core
- `Env`
  - Environment variable access and mutation.
  - Platform/process identity (OS, architecture, pid).
- `Process`
  - Synchronous subprocess execution with output capture and exit status.
- `JSON`
  - Parse and stringify structured values via RapidJSON.
  - Bridge JSON <-> Starbytes `Any`/`Array`/`Dict`/scalar values.
- `Log`
  - Structured logging with levels and dictionary fields.

Exit criteria:
- Each module ships with native implementation + `.starbint` interface.
- APIs are non-overlapping with `IO`/`FS`/`CmdLine`.
- Smoke coverage through full build + regression test pass.

### Phase 2: Network and Configuration
- `Net`: low-level socket/DNS/TLS primitives.
- `HTTP`: high-level request/response APIs over `Net`.
- `Config`: layered config loading/merge/validation.
- `Text`: advanced text transforms built on `Unicode` and builtin `Regex`.

### Phase 3: Security and Packaging Utilities
- `Random`: deterministic + secure randomness APIs.
- `Crypto`: hashes/KDF/signatures/HMAC.
- `Compression`: gzip/zstd-like stream helpers.
- `Archive`: tar/zip pack/unpack workflows.

### Phase 4: Release-Readiness Tooling
- `Test`: assertions, fixtures, snapshots.
- `DB.SQLite`: embedded storage module.
- `Metrics`: counters/timers/histograms for app/runtime instrumentation.

## Implementation Notes
- Reuse existing ADT abstractions from `include/starbytes/base/ADT.h` for common container/string handling in module implementations.
- Use RapidJSON from `deps/rapidjson` for JSON parsing/serialization instead of custom JSON classes.
- Keep interfaces in PascalCase module folders to match repository naming conventions.
