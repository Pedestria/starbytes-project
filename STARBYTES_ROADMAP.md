# Starbytes Completion Roadmap

Last updated: February 22, 2026

This roadmap turns Starbytes from a promising prototype into a complete interpreted language with a usable toolchain.


## Guiding Principles

- Keep language semantics coherent before adding many new features.
- Close parse + sema + codegen + runtime loops for each syntax surface.
- Ship in vertical slices with tests and diagnostics, not isolated parser-only additions.
- Treat language server and diagnostics as first-class developer experience requirements.

## Current Status Summary

Implemented language/runtime/tooling features include:

- Core declarations and expressions:
  - `decl` / `imut`
  - `func`, `return`
  - `if` / `elif` / `else`, `while`, `for`
  - arithmetic/comparison/logical operators, ternary, and compound assignment (`+=`, `-=`, `*=`, `/=`, `%=`).
  - inline functions and function type syntax.
- Type and error model:
  - optional (`?`) and throwable (`!`) qualifiers
  - `secure(... ) catch` handling
  - type aliases (`def`)
  - runtime type checking via `is` (supports aliases; interface targets are intentionally rejected).
- Object model:
  - classes, inheritance, constructors, fields/methods, attributes (`@native`, `@readonly`, `@deprecated`)
  - interfaces
  - structs
  - enums.
- Async/lazy surface:
  - `lazy` declarations
  - `await`
  - `Task<T>` type flow in semantic/codegen/runtime paths (single-threaded execution model).
- Data/collections:
  - array and dictionary literals
  - typed arrays (`T[]`) and implied array typing
  - inferred variable typing surfaced for literals, constructor calls, and invocation returns
  - configurable numeric literal inference mode (`--infer-64bit-numbers` for Long/Double defaults)
  - indexing and mutation.
- Regex:
  - runtime regex literal compilation with PCRE2 and catchable failures.
- Modules/imports/driver:
  - module graph discovery from file or directory
  - import resolution across local/module/stdlib folders
  - cycle detection diagnostics
  - `.starbmodpath` support for module search roots
  - advanced driver flags (`run`/`compile`/`check`, native loading flags, output controls, help/version, compile profiling output).
  - compile-time optimization track:
    - phase 1 instrumentation complete
    - phase 2 import/module analysis caching complete (content-hash + compiler-version + flags keyed)
    - phase 3 symbol resolution indexing complete (scope-aware lookup paths + stress coverage)
    - phase 4 allocation path consolidation complete for semantic symbol table objects.
    - phase 5 parallel module compilation complete (`--jobs` + dependency-aware task scheduling).
    - phase 6 incremental module compilation complete (fingerprint-aware module artifact reuse).
    - phase 7 tooling reuse complete (compiler-backed document/builtins analysis caching in LSP paths).
- Native stdlib integration:
  - module baselines for `IO`, `FS`, `CmdLine`, `Time`, `Threading`, `Unicode`, `Env`, `Process`, `JSON`, `Log`, `Config`, `Net`, `HTTP`, `Text`, `Random`, `Crypto`, `Compression`, and `Archive`.
  - auto-load and explicit `--native` support, plus runtime fallback behavior when optional third-party deps are unavailable.
- Tooling baseline:
  - LSP protocol surface implemented for core editor loop.
  - semantic highlighting generated from compiler pipeline.
  - richer hover details including declaration-adjacent comments.
  - inferred declaration types shown in LSP by using compiler-backed semantic symbol extraction (with syntax fallback merge for invalid code paths).
  - modularized LSP analysis pipeline (`DocumentAnalysis`) to reduce server coupling and duplicate parsing logic.
  - VSCode extension integration path for LSP workflow.
- Packaging/dev workflow:
  - `tools/starbpkg` package manager baseline in Starbytes with `.starbmodpath` generation.
  - library compile flow now emits `.starbint` interfaces with doc preservation; compile `--clean` removes module artifacts and local `.starbytes/.cache` outputs.

Known gaps and stability risks:

- Typed collection enforcement/inference is stabilized for current generic literal and typed-context surfaces; remaining work is concentrated in high-complexity nested generic/callable interactions.
- Advanced native `IO` stream methods and advanced `Unicode` operations are now stabilized with dedicated full-suite regression coverage (including former crash repro paths).
- LSP supports core features and semantic tokens, but reliability hardening and advanced refactor ergonomics are still pending.
- Diagnostics optimization phases 1-5 are complete (schema unification, source indexing/lazy rendering, dedup/cascade control, and renderer split), but full parser/sema/runtime error-code governance and incremental LSP diagnostic identity work are still pending.

## Phase Progress Snapshot

- M1 Core Language Closure: complete for current documented operator/control-flow surface.
- M2 Type System/Data Model: late stage; collection/generic hardening is broad, with only high-complexity edge interactions still open.
- M3 Modules and Imports: complete baseline with `.starbmodpath` and cache-backed import analysis.
- M4 Standard Library Baseline: expanded through phase 3 roadmap modules (network + security/compression/archive); phase 4 release-tooling modules remain.
- M5 Diagnostics: advanced progress with phases 1-5 completed; phases 6-8 (incremental identity, fix-it framework, hardening gates) remain.
- M6 LSP Maturity: late-mid stage (semantic tokens + richer hovers + compiler-backed symbols + inferred type surfacing + modular analysis split), with stability hardening and refactor ergonomics pending.
- M7 Syntax Surface Completion: mostly complete for currently documented syntax, with lambda ergonomics still the main unresolved area.
- M8 Tooling/Release Readiness: in progress with package manager baseline, full-suite regression expansion, compile profiling/benchmark support, and remaining CI/release hardening tasks.

## Version Status

- Current project version target: `0.10.0`.
- Rationale: Starbytes now includes complete compile-time optimization phases 1-7, diagnostics optimization phases 1-5, expanded stdlib modules through phase 3, stabilized IO/Unicode advanced paths with full-suite regression coverage, and stronger library/interface build workflows.

---

## Phase 1: Core Language Closure (M1)

Goal: make the core language internally complete and predictable.

### Scope

- Complete expression grammar and precedence.
- Implement missing operators end-to-end:
  - equality/inequality
  - comparison operators
  - logical operators
  - unary operators
  - compound assignments
- Finish plain variable assignment (`x = value`) in codegen/runtime.
- Finalize truthiness/bool rules for control-flow conditions.

### Deliverables

- Unified operator precedence parser.
- Runtime execution for all supported operators.
- Assignment behavior for vars and members with readonly enforcement.
- Spec section in docs for operator semantics and precedence.

### Exit Criteria

- All operators parse, type-check, generate bytecode, and execute.
- No parser/runtime divergence for any supported expression.
- Control-flow conditions pass dedicated semantic tests.
- 0 known TODO/FIXME in operator pipeline paths.

### Suggested Work Items

1. Build precedence table and parser tests first.
2. Expand RT opcode mapping and evaluation semantics.
3. Add assignment path for `ID_EXPR` in codegen/runtime.
4. Harden diagnostics for invalid operator/type combinations.

---

## Phase 2: Type System and Data Model Completion (M2)

Goal: finish practical typing for collections and callable surfaces.

### Scope

- Typed arrays and dictionaries:
  - element/value type checks
  - literal inference rules
  - indexing semantics
- Type alias system completion (`def` / `deft`) or remove from syntax until ready.
- Function type support (including callable signatures where declared).
- Improve type compatibility rules for optional/throwable interactions.

### Deliverables

- Collection type rules documented and enforced.
- Dictionary literal/index syntax closed end-to-end.
- Stable alias/function-type implementation or explicit deprecation.

### Exit Criteria

- Collection type mismatch tests fail with clear diagnostics.
- Indexing and mutation behavior is deterministic and covered.
- No “placeholder” type behavior in user-facing syntax paths.

---

## Phase 3: Modules and Imports (M3)

Goal: make multi-file projects first-class.

### Scope

- Define module resolution rules:
  - relative paths
  - standard library paths
  - import aliasing
- Implement import execution/linking semantics.
- Public symbol export/import model for functions/classes/scopes.
- Cycle detection and diagnostics for module graphs.

### Deliverables

- Working multi-file compile/run flow with imports.
- Formal import resolution and namespace behavior docs.
- CLI options for include/module search paths.

### Exit Criteria

- Integration tests with 3+ module graphs pass.
- Import errors show exact module path and location.
- Namespace collisions handled with deterministic diagnostics.

---

## Phase 4: Standard Library Baseline (M4)

Goal: provide enough batteries to build real programs.

### Scope

- Core stdlib modules:
  - strings
  - arrays/dicts helpers
  - regex API wrappers
  - file I/O
  - path utilities
  - time/date
  - process/args
- Define stdlib versioning and compatibility policy.

### Deliverables

- Initial `stdlib` module catalog with docs and examples.
- Runtime integration tests for each module.
- Error model alignment with `secure/catch`.

### Exit Criteria

- Small reference apps can run without host-language patching.
- Stdlib APIs are documented and semantically stable.

---

## Phase 5: Diagnostics and Developer Experience (M5)

Goal: deliver production-quality compiler/runtime feedback.

### Scope

- Expand diagnostics engine:
  - richer messages
  - exact source ranges
  - fix hints where possible
- Standardize error codes.
- Improve stack traces and runtime error context.
- Make diagnostics style consistent across parser/sema/runtime.

### Deliverables

- Error format spec and error-code registry.
- Golden-file diagnostic tests.
- Polished `CodeView` integration for visual snippets.

### Exit Criteria

- All major errors include source span + actionable message.
- No anonymous “error” fallbacks on known failure paths.
- Diagnostic output stable enough for editor integrations.

---

## Phase 6: LSP Maturity (M6)

Goal: raise Starbytes IDE support from basic to full workflow.

### Scope

- Add:
  - go-to-definition
  - hover type/info
  - references
  - rename symbol
  - document symbols
  - diagnostics push on change
- Move from regex-based tokenization to AST/symbol-backed intelligence.

### Deliverables

- LSP feature-complete baseline for everyday coding.
- VSCode extension integration for full language flow.

### Exit Criteria

- Typical edit loop (completion, jump, rename, diagnostics) works reliably.
- LSP behavior validated by editor-side integration tests.

---

## Phase 7: Syntax Surface Completion (M7)

Goal: either fully implement planned syntax or formally trim it.

### Scope

- Resolve status of currently partial/reserved constructs:
  - lambda syntax
  - function type syntax ergonomics
  - any remaining documented-but-partial constructs
- Decide keep/drop for each and align docs + parser + sema + runtime.

### Deliverables

- Finalized language surface for v1.0.
- No “documented-but-nonfunctional” syntax in primary docs.

### Exit Criteria

- Every documented keyword has end-to-end behavior.
- Deferred features clearly marked experimental/off by default.

---

## Phase 8: Tooling, Packaging, and Release Readiness (M8)

Goal: make Starbytes shippable and maintainable.

### Scope

- CLI ergonomics:
  - consistent compile/run commands
  - diagnostics output modes
  - module path flags
- Packaging/build:
  - deterministic builds
  - dependency bootstrap hardening (`init.py`)
  - CI matrix for macOS/Linux
- Quality gates:
  - parser/sema/runtime coverage thresholds
  - memory-safety and sanitizer runs

### Deliverables

- Release checklist and v1.0 RC process.
- Contributor/developer docs refresh.
- CI badges and status pipeline for main branches.

### Exit Criteria

- Clean CI on full test matrix.
- Reproducible local bootstrap/build/run path.
- Tagged release candidate with migration notes.

---

## Cross-Phase Quality Strategy

Apply to all phases:

- Add regression tests for every bug fix.
- Keep syntax docs in lockstep with implementation.
- Prefer feature flags for unfinished syntax.
- Enforce “no parser-only feature merges.”

## Proposed Milestone Order and Effort

1. M1 Core Language Closure: High impact, highest priority.
2. M2 Type/Data Model: High impact, high priority.
3. M3 Modules/Imports: High impact, high priority.
4. M5 Diagnostics: High impact, medium-high priority.
5. M6 LSP Maturity: Medium-high impact, medium priority.
6. M4 Stdlib Baseline: Medium-high impact, medium priority.
7. M7 Syntax Surface Completion: Medium impact, medium priority.
8. M8 Tooling/Release: Required for shipping, final gate.

## Immediate Next Sprint (Recommended)

Sprint objective: close release-hardening gaps in diagnostics/LSP/tooling (M5/M6/M8) while preserving current stability gains.

### Sprint Backlog

1. Implement diagnostics optimization phase 6 (incremental LSP diagnostics with stable diagnostic identities per URI/version).
2. Standardize parser/sema/runtime error-code families and enforce a shared registry policy.
3. Add LSP reliability hardening for advanced rename/references workflows, with dedicated regression coverage.
4. Complete stdlib expansion phase 4 modules (`Test`, `DB.SQLite`, `Metrics`) without overlapping existing builtin/module surfaces.
5. Finalize release hardening work: CI matrix coverage (macOS/Linux + sanitizer lanes), bootstrap reproducibility checks, and RC checklist docs.

### Sprint Done Definition

- LSP diagnostics publish incrementally with stable identities and no stale-cache regressions in integration tests.
- Parser/sema/runtime diagnostics use standardized, documented error-code families.
- Full suite remains green while adding phase 4 stdlib modules and new LSP/diagnostics regressions.
- Release checklist and CI matrix are executable end-to-end from a clean bootstrap environment.
