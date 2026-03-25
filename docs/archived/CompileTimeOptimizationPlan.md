# Compile Time Optimization Plan

Last updated: February 19, 2026

## Goal
Reduce Starbytes compile latency for iterative development while preserving output and diagnostics behavior.

## Success Metrics
- `p50` / `p95` elapsed time for `check`, `compile`, and `run`.
- Cold vs warm timings.
- Stage timings for:
  - module graph/import resolution
  - file load
  - lex
  - syntax parse
  - semantic analysis
  - AST consumer/codegen
  - code generation finalization

## Phase 1: Baseline and Instrumentation
Status: Complete

1. Add a driver flag `--profile-compile`.
2. Emit structured timing output (JSON-like) for key compile phases.
3. Add parser-internal timers for:
   - lex
   - syntax parse
   - semantic analysis
   - AST consumer/codegen
4. Add driver-side timers for:
   - module graph discovery
   - source file load
   - import scanning
   - import module resolution
5. Report counts:
   - modules, sources
   - parsed files, tokens, statements, source bytes

Completed notes:
- Implemented `--profile-compile` and `--profile-compile-out`.
- Added parser stage timers and driver module/import/file timing probes.
- Added profile fields for module/source and parser counts.

## Phase 2: Import/Module Caching
Status: Complete

1. Cache module analysis keyed by content hash + compiler version + flags.
2. Reuse unchanged import results across invocations.
3. Invalidate on dependency hash changes.

Completed notes:
- Added persistent module analysis cache at `<out-dir>/.cache/module_analysis_cache.v1`.
- Cache key includes source content hash + compiler version + resolver/flag hash.
- Import scan reuse confirmed on warm runs.
- Dependency/source edit invalidates cached imports automatically.

## Phase 3: Symbol Resolution Data Structures
Status: Complete

1. Move hot symbol lookups to per-scope hash maps.
2. Minimize linear scans in semantic checks.
3. Add micro-benchmarks for lookup-heavy files.

Completed notes:
- Added per-scope hash indexing in `Semantics::SymbolTable` (`scope -> symbol -> entries`).
- Added emitted-name hash indexing for fast emitted symbol reverse lookup.
- Replaced linear `findEntry*` scans in `STableContext` with indexed scope-chain lookups.
- Added `symbol-lookup-stress-test` to exercise lookup-heavy symbol resolution paths.

## Phase 4: Allocation and Memory
Status: Complete

1. Introduce arena allocation for AST/type lifetimes.
2. Reduce per-node heap churn in parse/semantic phases.

Completed notes:
- Added centralized owned allocation APIs on `Semantics::SymbolTable` for semantic entry/payload objects.
- Switched `SemanticA::addSTableEntryForDecl` to allocate symbol table entries/payloads via the table allocator path.
- Consolidated symbol allocation cleanup into `SymbolTable` teardown to reduce fragmented allocation/deallocation behavior.

## Phase 5: Parallel Module Compilation
1. Build import DAG.
2. Compile independent nodes concurrently.
3. Preserve deterministic diagnostics ordering.

## Phase 6: Incremental Compilation
1. Persist dependency graph + fingerprints.
2. Recompile changed modules and dependents only.

## Phase 7: Tooling Reuse
1. Reuse analysis across LSP requests for same document version.
