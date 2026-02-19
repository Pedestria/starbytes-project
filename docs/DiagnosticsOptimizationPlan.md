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
Scope:
- Standardize one internal diagnostic schema across parser/sema/runtime/LSP:
  - `id`, `severity`, `message`, `primary_span`, `related_spans`, `notes`, `fixits`, `phase`.
- Introduce stable error code families and compatibility adapters for existing producers.

## Phase 3: Span and Source Rendering Optimization
Scope:
- Build shared source/line index cache for span translation.
- Avoid repeated line/column recomputation.
- Add lazy `CodeView` rendering for non-human sinks.

## Phase 4: Deduplication and Cascade Control
Scope:
- Add diagnostic aggregation with duplicate suppression.
- Collapse repetitive cascades under root-cause diagnostics.
- Keep deterministic ordering.

## Phase 5: Renderer Split and Output Modes
Scope:
- Separate diagnostic data from rendering.
- Add terminal human renderer, compact machine JSON renderer, and direct LSP renderer.
- Keep formatting lazy until sink emission.

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
