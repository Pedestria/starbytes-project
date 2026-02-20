# Starbytes Diagnostics and Error-Code Standardization Plan

## Objective
Unify diagnostics across parser, semantic analyzer, and runtime so every error/warning uses one canonical schema, one stable code system, and one output path for CLI, tests, and LSP.

## Scope
- Parser diagnostics
- Semantic diagnostics
- Runtime diagnostics (including native/interop boundaries)
- Driver and LSP diagnostic transport/output

## Non-Goals
- Rewriting all diagnostic message text in one pass
- Changing language semantics
- Removing backward-compatible output immediately

## Guiding Principles
- Every diagnostic has a stable, unique code.
- Every diagnostic is emitted through one shared pipeline.
- Human output and machine output are derived from the same record.
- Error ordering is deterministic.

## Canonical Diagnostic Contract
Create a shared record type used by all phases.

```cpp
struct DiagnosticRecord {
    std::string code;          // ex: SBP0007
    DiagnosticSeverity severity; // Error, Warning, Info, Hint
    std::string phase;         // parser | sema | runtime | infra
    std::string message;
    std::string source;        // file/module path
    Region region;             // start/end line+column
    std::vector<DiagnosticNote> notes;
    std::optional<std::string> hint;
    std::optional<std::string> symbol;
    std::vector<StackFrame> stack; // optional, runtime-heavy cases
};
```

## Error-Code Taxonomy
Use namespaced, stable codes:

- `SBP####` parser
- `SBS####` semantic analyzer
- `SBR####` runtime
- `SBI####` infrastructure/driver/internal

### Reserved Ranges
- `SBP0001-0999`: syntax/tokenization/parser
- `SBS1000-1999`: type/name/scope/semantic checks
- `SBR2000-2999`: runtime execution/native/interop
- `SBI9000-9999`: toolchain/internal errors

## Central Registry
Add a single source-of-truth registry file (JSON/YAML/def-table) with:
- code
- short title
- long description
- default severity
- phase
- deprecation status

All emitted diagnostics must map to an entry in this registry.

## Shared Pipeline Architecture
1. `DiagnosticFactory`: constructs `DiagnosticRecord` using registry keys.
2. `DiagnosticHandler`: stores, sorts, deduplicates, and emits.
3. `DiagnosticRenderer`:
- human renderer (existing style, now code-prefixed)
- JSON renderer (`--diagnostics-format json`)
4. LSP adapter maps `DiagnosticRecord` -> LSP diagnostics.

## Migration Plan

### Phase A: Foundation (no behavior change)
- Introduce `DiagnosticRecord` and registry.
- Add adapter from legacy diagnostics to `DiagnosticRecord`.
- Keep existing output format, append code prefix.

### Phase B: Parser migration
- Replace parser free-form emissions with code-backed builder helpers.
- Map frequent errors first: unexpected token, malformed type, invalid declaration syntax.

### Phase C: Semantic migration
- Migrate semantic errors to standardized codes.
- Cover: unknown symbol/type, type mismatch, invalid member access, invalid arity, invalid interface/class rules.

### Phase D: Runtime migration
- Wrap runtime/native exceptions into canonical records.
- Add stack frame capture where available.
- Normalize native failure payloads into `notes`.

### Phase E: Strict enforcement
- CI fails if unknown/unregistered code is emitted.
- Remove legacy adapter path after one compatibility release.

## Output Standardization
- Default CLI: human-readable diagnostic output with code.
- Optional machine mode: `--diagnostics-format json`.
- Keep region/source formatting consistent across phases.
- Keep deterministic ordering: `source -> line -> col -> phase -> code`.

## Testing and Quality Gates

### New Tests
- Registry completeness: every emitted code exists in registry.
- Snapshot tests for parser/sema/runtime representative failures.
- Ordering determinism tests (same input => same diagnostic order).
- Severity validation tests.

### CI Gates
- No unknown codes.
- No duplicate code definitions.
- No phase/code mismatch.

## Rollout and Compatibility
- Maintain current output compatibility during Phases A-D.
- Provide a temporary flag for legacy formatting if needed.
- Announce final strict cutover in release notes.

## Acceptance Criteria
- 100% parser/sema/runtime diagnostics have standardized codes.
- Single diagnostic schema used end-to-end.
- CLI/LSP derive diagnostics from the same canonical records.
- Deterministic diagnostics in all regression suites.
- CI enforces registry integrity and code usage.

## Proposed Implementation Order in Repository
1. Shared diagnostics types and registry in compiler/base layer.
2. Driver flag for diagnostics format (`text|json`).
3. Parser migration.
4. Semantic migration.
5. Runtime migration.
6. LSP adapter switch to canonical records.
7. CI gating and legacy-path removal.
