# Diagnostics Optimization Plan

Last updated: February 19, 2026

## Goal
Improve diagnostics quality and performance together: faster emission/rendering, better consistency across parser/sema/runtime/LSP, and stable output for tooling.

## Success Metrics
- Diagnostics emitted per second in parser/sema-heavy workloads.
- Average render time per diagnostic.
- Duplicate/cascade diagnostic rate.
- Span accuracy rate for primary locations.
- LSP diagnostic publish latency after text changes.

## Phase 1: Baseline and Measurement
Status: Implemented (initial baseline)

Scope:
- Add measurable diagnostics handler metrics.
- Capture counts and timing in core diagnostic lifecycle paths.
- Add baseline diagnostic regression test with stable assertions.

Implemented:
- `DiagnosticHandler::Metrics` in `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/base/Diagnostic.h`:
  - `pushedCount`, `renderedCount`, `errorCount`, `warningCount`
  - `withLocationCount`, `skippedResolvedCount`
  - `pushTimeNs`, `renderTimeNs`, `hasErroredTimeNs`
  - `maxBufferedCount`
- Metrics lifecycle methods:
  - `getMetrics()`
  - `resetMetrics()`
- Instrumentation in `/Users/alextopper/Documents/GitHub/starbytes-project/src/base/Diagnostic.cpp`:
  - metrics updates in `push`, `hasErrored`, and `logAll`.
- Baseline regression test:
  - `/Users/alextopper/Documents/GitHub/starbytes-project/tests/DiagnosticBaselineTest.cpp`
  - validates counts, timing fields, and baseline output markers.

Exit criteria:
- Metrics can be queried and reset.
- Baseline test passes in CI.
- No runtime regressions in existing parse/compile/run/LSP tests.

## Phase 2: Unified Diagnostic Schema
Status: Implemented

Scope:
- Standardize one internal diagnostic schema across parser/sema/runtime/LSP:
  - `id`, `severity`, `message`, `primary_span`, `related_spans`, `notes`, `fixits`, `phase`.
- Introduce stable error code families and compatibility adapters for existing producers.

Implemented:
- Extended `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/base/Diagnostic.h` schema:
  - `id`, `code`, `phase`, `relatedSpans`, `notes`, `fixits`.
- Added compatibility adapters in `/Users/alextopper/Documents/GitHub/starbytes-project/src/base/Diagnostic.cpp`:
  - automatic phase/source backfill from handler defaults.
  - stable code/id generation for legacy producers.
- Producer integration:
  - parser defaults + parser error code in `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/Parser.cpp`.
  - semantic producer tagging/code family in `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp`.
  - runtime default phase/source initialization in `/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/main.cpp`.
- LSP schema propagation:
  - enriched diagnostic payload in `/Users/alextopper/Documents/GitHub/starbytes-project/tools/lsp/DocumentAnalysis.h`.
  - mapping from compiler diagnostics in `/Users/alextopper/Documents/GitHub/starbytes-project/tools/lsp/DocumentAnalysis.cpp`.
  - JSON emission (`code`, `relatedInformation`, `data.phase`, `data.id`, `data.notes`, `data.fixits`) in `/Users/alextopper/Documents/GitHub/starbytes-project/tools/lsp/ServerMain.cpp`.

## Phase 3: Span and Source Rendering Optimization
Status: Implemented

Scope:
- Build shared source/line index cache for span translation.
- Avoid repeated line/column recomputation.
- Add lazy `CodeView` rendering for non-human sinks.

Implemented:
- Shared source/line index cache in `/Users/alextopper/Documents/GitHub/starbytes-project/src/base/CodeView.cpp` with cached line-start tables reused across identical sources.
- `CodeView` storage switched to indexed source representation in `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/base/CodeView.h`.
- Lazy rendering mode for non-human sinks:
  - `DiagnosticHandler::OutputMode` (`Human`, `Machine`) in `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/base/Diagnostic.h`.
  - machine mode disables `CodeView` source binding/rendering in `/Users/alextopper/Documents/GitHub/starbytes-project/src/base/Diagnostic.cpp`.
  - LSP diagnostics collection uses machine mode in `/Users/alextopper/Documents/GitHub/starbytes-project/tools/lsp/DocumentAnalysis.cpp`.

## Phase 4: Deduplication and Cascade Control
Status: Implemented

Scope:
- Add diagnostic aggregation with duplicate suppression.
- Collapse repetitive cascades under root-cause diagnostics.
- Keep deterministic ordering.

Implemented:
- Aggregation and normalization pipeline in `/Users/alextopper/Documents/GitHub/starbytes-project/src/base/Diagnostic.cpp`:
  - duplicate suppression keyed by source/phase/severity/code/location/message.
  - cascade collapse of `Context:` / `Related:` diagnostics into root diagnostic notes.
  - deterministic ordering by source and span coordinates.
- Phase 4 controls and metrics in `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/base/Diagnostic.h`:
  - `setDeduplicationEnabled`, `setCascadeCollapseEnabled`.
  - `deduplicatedCount`, `cascadeSuppressedCount`.
- Data-oriented accessors in diagnostics API:
  - `collectRecords(...)` and `collectLspRecords(...)`.

## Phase 5: Renderer Split and Output Modes
Status: Implemented

Scope:
- Separate diagnostic data from rendering.
- Add terminal human renderer, compact machine JSON renderer, and direct LSP renderer.
- Keep formatting lazy until sink emission.

Implemented:
- Renderer split in `/Users/alextopper/Documents/GitHub/starbytes-project/src/base/Diagnostic.cpp`:
  - `renderHumanDiagnosticRecord(...)`
  - `renderMachineJsonDiagnosticRecord(...)`
- Output mode expansion in `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/base/Diagnostic.h`:
  - `OutputMode::{Human, MachineJson, Lsp}` (+ `Machine` alias).
- LSP direct diagnostic record integration in `/Users/alextopper/Documents/GitHub/starbytes-project/tools/lsp/DocumentAnalysis.cpp`:
  - compiler diagnostics mapped from `collectLspRecords()` records, without legacy pointer snapshot shaping.
- Regression coverage in `/Users/alextopper/Documents/GitHub/starbytes-project/tests/DiagnosticBaselineTest.cpp`:
  - dedup/cascade behavior and deterministic order checks.
  - machine JSON output and phase 4/5 metrics assertions.

## Phase 6: LSP Incremental Diagnostics
Scope:
- Reuse compiler diagnostics for open-doc versions.
- Publish incrementally by URI/version.
- Maintain stable diagnostic identities for editor diffing.

## Phase 7: Fix-it Framework
Scope:
- Add structured fix suggestions for frequent errors.
- Start with high-confidence categories (imports, type mismatch, symbol resolution).

## Phase 8: Hardening and Gates
Scope:
- Golden-file diagnostics suite expansion.
- Fuzz/property tests for spans and rendering safety.
- CI regression gates on latency, duplication, and span correctness.
