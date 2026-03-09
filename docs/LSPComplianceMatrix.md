# Starbytes LSP Feature and Compliance Matrix

Date: March 9, 2026  
Server: `starbytes-lsp` (version `0.11.0`)

## Scope

This matrix reflects the current implementation in:
- `tools/lsp/ServerMain.cpp`
- `tools/lsp/ServerMain.h`

Status labels:
- `Implemented`: capability advertised and backed by non-stub handler behavior.
- `Partial`: implemented with known semantic/protocol limitations.
- `Missing`: not implemented (or not advertised and only returns empty fallback).

---

## Advertised Capabilities (`initialize` result)

| Capability | Status | Notes |
|---|---|---|
| `textDocumentSync` (`openClose`, incremental change, save includeText) | Implemented | Open/change/save/close wired. |
| `completionProvider` + resolve | Implemented | Trigger chars: `.` and `:`. Scoped completion uses cached semantic parse for locals, modules, scopes, and members. |
| `hoverProvider` | Implemented | Symbol/builtin/doc rendering. |
| `definitionProvider` | Implemented | Scoped/local/class/scope/import/builtin resolution. |
| `declarationProvider` | Partial | Aliases definition logic. |
| `typeDefinitionProvider` | Partial | Aliases definition logic. |
| `implementationProvider` | Partial | Semantic implementation lookup for class/interface inheritance and method implementations; falls back to definition for unsupported symbol kinds. |
| `referencesProvider` | Partial | Scoped semantic identity across loaded documents; still occurrence-scan based and not project-index driven. |
| `documentSymbolProvider` | Partial | Flat list, no symbol tree hierarchy. |
| `workspaceSymbolProvider` | Partial | Loaded-document scan, not full project index. |
| `renameProvider` (`prepareProvider=true`) | Partial | Lexical rename, no deep semantic safety. |
| `signatureHelpProvider` | Implemented | Trigger/retrigger `(` and `,`. |
| `documentHighlightProvider` | Implemented | Identifier occurrence highlighting. |
| `documentFormattingProvider` | Implemented | Uses shared `FormatterEngine`. |
| `documentRangeFormattingProvider` | Partial | Uses full-doc canonical formatting edit. |
| `foldingRangeProvider` | Implemented | Region folding from braces. |
| `selectionRangeProvider` | Implemented | Word + line parent range. |
| `codeActionProvider` | Implemented | Uses shared linguistics code actions. |
| `semanticTokensProvider` (`full`, `range`, `delta`) | Implemented | Full/range/delta wired. |
| `executeCommandProvider` | Partial | Advertised, but command list is empty and returns null. |

---

## Request/Notification Coverage

### Lifecycle and protocol controls

| Method | Status | Notes |
|---|---|---|
| `initialize`, `initialized`, `shutdown`, `exit` | Implemented | Standard lifecycle flow. |
| `$/cancelRequest` | Partial | Cancellation registered; no deep cooperative cancellation in all heavy tasks. |
| `$/setTrace`, `$/logTrace` | Implemented | Basic trace level handling. |
| `window/workDoneProgress/create` | Partial | Supported as null response only. |

### Sync and diagnostics

| Method | Status | Notes |
|---|---|---|
| `textDocument/didOpen`, `didChange`, `didSave`, `didClose` | Implemented | Updates document state and publishes diagnostics. |
| `textDocument/publishDiagnostics` (push) | Implemented | Compiler + linguistics diagnostics published. |
| `textDocument/diagnostic` (pull) | Implemented | Full report with compiler + linguistics diagnostics. |
| `workspace/diagnostic` (pull) | Implemented | Full reports per loaded document. |

### Language intelligence

| Method | Status | Notes |
|---|---|---|
| `textDocument/completion` | Implemented | Keywords + scoped semantic completion for locals, imports, scopes, and receiver members; builtin member-aware. |
| `completionItem/resolve` | Implemented | Uses exact symbol identity when present in completion item data; label-based fallback remains. |
| `textDocument/hover` | Implemented | Signature/docs/inheritance/type-oriented hover text. |
| `textDocument/definition` | Implemented | Scoped symbol + builtin resolution path. |
| `textDocument/declaration` | Partial | Definition alias. |
| `textDocument/typeDefinition` | Partial | Definition alias. |
| `textDocument/implementation` | Partial | Scoped resolver plus inheritance/method implementation search for loaded documents. |
| `textDocument/references` | Partial | Scoped resolver + semantic declaration-identity filtering across loaded documents. |
| `textDocument/documentSymbol` | Partial | Flat symbols only. |
| `workspace/symbol` | Partial | No global persisted index walk. |
| `textDocument/prepareRename`, `textDocument/rename` | Partial | Lexical rename; no semantic symbol ownership checks. |
| `textDocument/signatureHelp` | Implemented | Function signature extraction/lookup. |
| `textDocument/documentHighlight` | Implemented | Occurrence highlighting. |
| `textDocument/formatting` | Implemented | Linguistics formatter-backed edits. |
| `textDocument/rangeFormatting` | Partial | Full-document replacement model for range request. |
| `textDocument/codeAction` | Implemented | Quick-fix/code-action output from linguistics pipeline. |
| `textDocument/foldingRange` | Implemented | Brace-driven folding output. |
| `textDocument/selectionRange` | Implemented | Word/line range chain. |
| `textDocument/semanticTokens/full`, `range`, `full/delta` | Implemented | All three handlers present and active. |

### Compatibility fallback handlers (not fully implemented features)

| Method | Status | Notes |
|---|---|---|
| `codeLens/resolve`, `documentLink/resolve` | Partial | Pass-through response only. |
| `textDocument/codeLens` | Missing | Returns empty array fallback. |
| `textDocument/documentLink` | Missing | Returns empty array fallback. |
| `textDocument/inlayHint` | Missing | Returns empty array fallback. |
| `textDocument/linkedEditingRange` | Missing | Returns empty array fallback. |
| `textDocument/inlineValue` | Missing | Returns empty array fallback. |
| `textDocument/documentColor` | Missing | Returns empty array fallback. |
| `textDocument/colorPresentation` | Missing | Returns empty array fallback. |
| `textDocument/prepareCallHierarchy` | Missing | Returns empty array fallback. |
| `callHierarchy/incomingCalls`, `callHierarchy/outgoingCalls` | Missing | Returns empty array fallback. |
| `typeHierarchy/prepare`, `typeHierarchy/supertypes`, `typeHierarchy/subtypes` | Missing | Returns empty array fallback. |
| `textDocument/moniker` | Missing | Returns empty array fallback. |
| `textDocument/willSaveWaitUntil` | Missing | Returns empty array fallback. |

---

## Compliance Gaps to “Full” LSP 3.17-Level Behavior

### High-impact remaining gaps

1. Split `declaration` and `typeDefinition` into distinct semantic resolvers instead of definition aliases.
2. Upgrade `rename` to semantic symbol identity and ownership checks, matching the newer scoped references path.
3. Upgrade `documentSymbol` to hierarchical `DocumentSymbol` trees where applicable.
4. Add `diagnosticProvider` capability negotiation semantics and incremental `resultId` lifecycle quality.
5. Add richer `workspace/executeCommand` command registration + execution.
6. Broaden `implementation` beyond current class/interface/method inheritance cases.

### Medium-impact gaps

1. Add on-type formatting support (`textDocument/onTypeFormatting`) if desired.
2. Add inlay hints, code lens, document links, linked editing, call/type hierarchy families.
3. Add partial-result/workDone progress wiring for heavy operations.
4. Add explicit position encoding negotiation and conformance tests.

### Operational hardening already present

- Shared linguistics pipeline wired into diagnostics, formatting, and code actions.
- Incremental per-document linguistics caches in LSP server state.
- Shared semantic resolved-document cache now backs hover, completion, definition, references, and implementation.
- Optional LSP profiling hooks (`STARBYTES_LSP_PROFILE`) with summary at shutdown.

---

## Practical “Compliant Release” Criteria

Starbytes LSP can be treated as release-ready for common editor workflows when:
1. All currently advertised capabilities keep non-stub behavior.
2. Semantic correctness is completed for rename/declaration/typeDefinition and the remaining implementation edge cases.
3. Diagnostic pull semantics (`resultId` behavior + capability negotiation) are standardized.
4. Optional advanced features remain unadvertised until implemented.
