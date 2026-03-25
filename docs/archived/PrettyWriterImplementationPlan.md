# Starbytes Linguist Tool and Linguistics Library Plan

Last updated: February 28, 2026

## Goal
Deliver a full linguistics subsystem for Starbytes as a reusable library under `src/linguistics`, consumed by both:
- `starbytes-ling` (CLI linguist tool)
- `starbytes-lsp` (LSP server features)

This subsystem includes:
- pretty writer (formatter)
- linting
- suggestions
- code actions / quick fixes

## Current Thin Slice (Implemented)
- `starbytes-ling` tool exists.
- `--pretty-write` now uses compiler-backed AST formatting with safety gates.
- Current fallback behavior:
  - comments/trivia-sensitive sources fall back to preview normalization until phase 4
  - unsupported round-trip keywords (`struct`, `enum`) fall back to preview normalization until phase 3
  - parse-after-format and idempotence checks guard output safety

## Target Architecture
## 1) Shared Library Placement
Create a dedicated library layer:
- Headers: `include/starbytes/linguistics/*.h`
- Sources: `src/linguistics/*.cpp`

No linguistics logic should live only in the tool or only in the LSP.
Both must call shared library APIs.

## 2) Core Components
- `LinguisticsSession`
  - owns source text, parse context, semantic context, and caches.
- `FormatterEngine`
  - produces canonical source text and text edits.
- `LintEngine`
  - runs rule sets and produces diagnostics + fix candidates.
- `SuggestionEngine`
  - emits ranked non-error suggestions (quality/readability guidance).
- `CodeActionEngine`
  - maps diagnostics/suggestions to concrete edits and command actions.
- `LinguisticsConfig`
  - merges defaults, file config, and CLI/LSP overrides.
- `LinguisticsSerializer`
  - stable JSON/plain-text output encoding for tool and LSP transport.

## 3) Shared Data Contracts
- `TextSpan`: source range abstraction.
- `TextEdit`: `range + replacement`.
- `LintFinding`:
  - `id`, `code`, `severity`, `message`, `span`, `notes`, `related`, `fixes`, `tags`.
- `Suggestion`:
  - `id`, `kind`, `title`, `message`, `span`, `confidence`, `edits`.
- `CodeAction`:
  - `id`, `title`, `kind`, `diagnosticRefs`, `edits`, `preferred`, `isSafe`.

## 4) Rule Categories
- `style`: formatting consistency and readability rules.
- `correctness`: suspicious but compilable constructs.
- `performance`: avoidable runtime/compile-time costs.
- `safety`: risky patterns and unsafe assumptions.
- `docs`: declaration documentation quality checks.

## CLI Surface (`starbytes-ling`)
## Implemented
- `--pretty-write <file>`

## Planned
- Formatting:
  - `--pretty-write <file|dir>`
  - `--pretty-write-in-place`
  - `--pretty-check`
  - `--pretty-config <path>`
  - `--pretty-max-width <n>`
- Linting:
  - `--lint <file|dir>`
  - `--lint-strict`
  - `--lint-enable <rule[,rule...]>`
  - `--lint-disable <rule[,rule...]>`
  - `--lint-max <n>`
- Suggestions and actions:
  - `--suggest <file|dir>`
  - `--code-actions <file|dir>`
  - `--apply-safe-fixes`
  - `--apply-fix <action-id>`
- Output:
  - `--json`
  - `--color <auto|always|never>`

## LSP Integration Plan
All linguistics-powered LSP behavior must be sourced from `src/linguistics`.

- Diagnostics:
  - LSP publishes parser/sema diagnostics + lint diagnostics from `LintEngine`.
- Formatting:
  - LSP document/range formatting calls `FormatterEngine` for text edits.
- Code actions:
  - LSP `textDocument/codeAction` uses `CodeActionEngine`.
- Quick fixes:
  - Diagnostic fix-its are surfaced as preferred code actions.
- Suggestions:
  - Non-error suggestions surfaced as hints/information diagnostics and actions.

## Detailed Delivery Phases
## Phase 1: Foundation and API Contracts
Status: Implemented
- `starbytes-ling` tool introduced.
- Introduce shared `src/linguistics` + `include/starbytes/linguistics` library layer.
- Move formatter preview logic into `FormatterEngine` in shared linguistics library.
- Define stable contracts (`LintFinding`, `Suggestion`, `CodeAction`, `TextEdit`).
- Add baseline scaffolding engines (`LintEngine`, `SuggestionEngine`, `CodeActionEngine`) and serializer.
- Wire LSP server state to shared linguistics configuration/engine members for phase 2 integration.

## Phase 2: Formatter Core
Status: Implemented (phase 2 baseline)
- Build compiler-backed document IR and renderer.
- Replace preview-only normalization with AST/trivia-aware formatting.
- Add idempotence guarantee and parse-after-format safety gate.

## Phase 3: Lint Engine Core
Status: Implemented (phase 3 baseline)
- Add rule registry and rule metadata.
- Implement baseline rules in each category (`style`, `correctness`, `performance`, `safety`, `docs`).
- Emit deterministic, code-based findings aligned with diagnostics standardization.

## Phase 4: Suggestions and Code Actions
Status: Implemented (phase 4 baseline)
- Implement suggestion ranking and confidence scoring.
- Generate safe quick fixes with deterministic edit ordering.
- Add CLI apply modes with dry-run preview.

## Phase 5: LSP Wiring
Status: Implemented (phase 5 baseline)
- Route formatting provider through `FormatterEngine`.
- Route diagnostics and code actions through linguistics engines.
- Ensure no duplicate rule logic remains in `tools/lsp`.

## Phase 6: Workspace and Module Workflows
Status: Implemented (phase 6 baseline)
- Directory/module traversal support for all linguistics operations.
- Include/exclude globs and `.starbmodpath` aware resolution.
- Incremental caching for repeated runs in large projects.

## Phase 7: Hardening and Performance
Status: Implemented (phase 7 baseline)
- Golden tests for formatter/lint/actions output.
- Regression suite for malformed/extreme inputs.
- Performance gates for large files and large workspaces.
- Memory and latency profiling for LSP interactive usage.

## Testing Plan
- Unit tests:
  - formatter document model and rendering decisions
  - lint rule execution and finding stability
  - suggestion/action generation and edit conflict handling
- Integration tests:
  - `starbytes-ling` stdout and JSON snapshots
  - in-place and check modes
  - safe fix application correctness
- LSP tests:
  - diagnostics parity with CLI lint mode
  - code action round-trip correctness
  - document/range formatting behavior
- Extreme cases:
  - huge files, deep nesting, mixed comments, malformed syntax, overlapping edits

## Success Criteria
- One shared linguistics core powers both CLI and LSP.
- Formatter is deterministic and idempotent.
- Lint findings are stable, coded, and actionable.
- Code actions are safe by default and test-covered.
- LSP latency remains interactive on typical project sizes.

## Non-Goals
- Rewriting compiler semantic rules inside linguistics.
- Introducing duplicate parser/lexer implementations.
- Performing semantic-changing rewrites under automatic safe-fix modes.
