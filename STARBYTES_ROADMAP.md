# Starbytes Completion Roadmap

Last updated: February 18, 2026

This roadmap turns Starbytes from a promising prototype into a complete interpreted language with a usable toolchain.

## Guiding Principles

- Keep language semantics coherent before adding many new features.
- Close parse + sema + codegen + runtime loops for each syntax surface.
- Ship in vertical slices with tests and diagnostics, not isolated parser-only additions.
- Treat language server and diagnostics as first-class developer experience requirements.

## Current Status Summary

Implemented foundations include:

- Lexer/parser for core declarations and expressions.
- Classes, constructors, field initializers, scopes (`scope`), attributes.
- Optional (`?`) and throwable (`!`) type qualifiers with `secure(... ) catch`.
- Runtime regex literals compiled with PCRE2.
- Basic LSP completion/highlighting.

Primary missing areas:

- Full expression operator semantics (`==`, `!=`, logical ops, compound assignment, etc.).
- Plain variable assignment codegen/runtime completion.
- Full collection semantics (typed arrays/dicts, indexing, mutation ops).
- Complete module/import behavior.
- Broader standard library and mature tooling capabilities.

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
  - `interface`
  - `struct`
  - `enum`
  - lambda syntax
  - async/lazy/await
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

Sprint objective: finish M1.

### Sprint Backlog

1. Implement precedence-based expression parser.
2. Wire operator opcodes in codegen/runtime.
3. Add plain variable assignment emission and execution.
4. Add operator/type mismatch diagnostics with source spans.
5. Build focused tests:
   - operator matrix tests
   - assignment tests
   - control-flow predicate tests

### Sprint Done Definition

- `tests` includes full operator coverage for implemented syntax.
- No known mismatch between parser-recognized operators and runtime support.
- Docs updated with final operator precedence and behavior.

