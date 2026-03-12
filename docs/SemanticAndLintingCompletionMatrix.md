# Semantic And Linting Completion Matrix

Last updated: March 11, 2026

## Purpose

This document audits the current Starbytes semantic analyzer and lint engine, then proposes the additional checking surface needed to make both engines feel complete for the current language.

The goal is not to duplicate the two engines. The clean boundary is:

- semantic analyzer: reject invalid programs and enforce language/runtime rules
- lint engine: flag suspicious, risky, or maintainability-hostile code that is still legal

## Basis

This matrix is based on the implemented behavior in:

- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/LintEngine.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/linguistics/LintEngine.h`
- `/Users/alextopper/Documents/GitHub/starbytes-project/tests/LinguisticsPhase3LintTest.cpp`

## Current State Summary

- The semantic analyzer is already substantial. It covers most declaration validity, type checking, generic specialization, member resolution, inheritance/interface conformance, and callable validation.
- The lint engine is still foundational. It has rule metadata, severity handling, rule selection, and fix plumbing, but the rule set is small and mostly text-driven rather than AST- and sema-driven.
- The highest-leverage completion step is not "add fifty more regex-like lint rules". It is to let lint consume compiler AST and semantic facts first.

## Current Semantic Analyzer Coverage

| Area | Current implemented checks | Status | Main gap remaining |
| --- | --- | --- | --- |
| Name resolution and scoping | Lexical scope lookup, class/scope member lookup, module-scoped imports, qualified imported symbol enforcement, undefined symbol/member diagnostics, unused import warnings | Strong | No full shadowing policy diagnostics |
| Declaration validity | `const` initialization, typed/untyped declaration validation, secure declaration initializer constraints, `return` outside function errors, constructor return rejection, definite-assignment diagnostics, unused local/parameter warnings | Strong | No broader field/property definite-init policy |
| Attribute validation | `@native`, `@readonly`, `@private`, `@deprecated` declaration-shape validation plus deprecation use warnings in sema | Implemented | Deprecation payloads are surfaced at use sites |
| Type checking | Arithmetic, logical, bitwise, comparison, ternary, cast, `await`, indexing, assignment, optional/throwable matching, typed collection matching | Strong | No flow-sensitive narrowing beyond current direct checks |
| Callable checking | Function/method/constructor/inline-function arity checks, argument type checks, return type compatibility, generic callable specialization and inference, must-return control-flow diagnostics for non-`Void` callables | Strong | No richer override contract checking |
| Generics | Generic types, free functions, methods, interface methods, defaults, constructor generics, substitution, inference from call arguments | Strong | No constraints/where-clause enforcement yet |
| Object model | Single-superclass validation, circular inheritance rejection, duplicate method detection, duplicate constructor arity rejection, interface method signature compatibility | Strong | No richer override contract checking (`override` required/invalid override/covariant diagnostics) |
| Control-flow warnings | Unreachable code after a proven `return`, unused invocation result warning for non-`Void` calls | Basic | No CFG-backed dead-store, constant-condition, or must-return analysis |
| Modules and native interfaces | Interface/native declaration body rules, imported module namespace rules, native variable restrictions, module-scoped symbol surfaces | Strong | No API drift checking between source modules, interfaces, and native exports |
| Diagnostics shape | Errors and warnings are emitted consistently through semantic diagnostics | Adequate | Codes are still broad; related families are not fine-grained by check kind |

## Current Lint Engine Coverage

### Engine capabilities already implemented

- rule registry with categories, tags, default severities, and stable rule codes
- `strict` mode severity escalation
- enabled/disabled rule selection
- maximum finding caps
- fix candidate plumbing for suggestions/code actions

### Implemented rules

| Rule | Code | Category | Current behavior | Limit |
| --- | --- | --- | --- | --- |
| `style.trailing_whitespace` | `SB-LINT-STYLE-S0001` | Style | Detects and autofixes trailing spaces/tabs | Text-only |
| `correctness.assignment_in_condition` | `SB-LINT-CORR-C0001` | Correctness | Detects likely accidental `=` inside conditions from parsed AST | No richer constant-condition reasoning yet |
| `performance.new_in_loop` | `SB-LINT-PERF-P0001` | Performance | Detects constructor allocation inside loop bodies from parsed AST | No cost model beyond direct allocation sites |
| `safety.untyped_catch` | `SB-LINT-SAFE-A0001` | Safety | Detects broad/untyped `catch` clauses from parsed AST | No distinction between intentionally broad catch policies yet |
| `docs.missing_decl_comment` | `SB-LINT-DOC-D0001` | Docs | Detects missing declaration comments on top-level API declarations | Still shallow; no param/return doc drift checks |
| `correctness.shadowing` | `SB-LINT-CORR-C0004` | Correctness | Detects locals/catches that shadow outer bindings | No policy knobs yet for “allowed” shadowing cases |

### Current lint engine gap

The lint engine now consumes:

- parsed AST statements, including sema-invalid statements preserved through the raw consumer path
- lexical binding and block-structure facts derived from compiler AST
- declaration comments and callable signatures from compiler nodes

The lint engine still does not yet consume:

- symbol identities
- inferred types
- module/import ownership
- full control-flow facts
- semantic diagnostics as rule inputs

Without those inputs, the lint engine will continue to miss real issues and produce brittle heuristics.

## Completion Boundary

Use this split to decide where a new check belongs:

### Semantic analyzer should own

- anything that changes whether a program is valid
- anything needed for runtime safety or soundness
- anything that depends on exact type rules or symbol identity

### Lint engine should own

- suspicious but legal patterns
- maintainability/performance guidance
- team policy and public API hygiene
- style/documentation checks

## Proposed Semantic Analyzer Completion Features

### Priority 0: high-value soundness and correctness checks

| Feature | Why it matters | Notes |
| --- | --- | --- |
| Definite assignment / use-before-initialization | Prevents reading locals, fields, and catches before they are written on every path | Requires basic control-flow graph or block-path lattice |
| Must-return analysis for non-`Void` callables | Current sema checks return type compatibility when a `return` exists, but not that all paths return | Should cover functions, methods, inline functions, and interface default bodies |
| Unused imports / locals / parameters / private fields | High-signal correctness and cleanup feedback | Implemented in sema as warnings; `@private` fields warn when never read |
| Shadowing diagnostics | Prevent accidental reuse of local/parameter/catch/import names | Severity should depend on scope distance and symbol kind |
| Deprecation usage warnings | Use of deprecated variables, functions, classes, interfaces, type aliases, fields, and methods is surfaced from sema with message payloads | Implemented |
| Constant-condition and impossible-branch detection | Catches `if true`, `while false`, contradictory checks, and always-true type tests | Split hard errors vs warnings carefully |
| Invalid override and missing override contract checks | Makes class evolution safer and diagnostics more actionable | Covers accidental overload-vs-override mistakes |
| Stronger optional/throwable flow refinement | Makes `secure`, guards, and checked paths narrow types predictably | Important for optional/throwable-heavy APIs |

### Priority 1: deeper semantic quality checks

| Feature | Why it matters | Notes |
| --- | --- | --- |
| Dead-store detection | Finds writes that are overwritten before read | Best implemented as semantic warning or sema-fed lint |
| Lossy numeric conversion warnings | Helps catch narrowing and mixed numeric pitfalls | Especially relevant now that `Long` and `Double` exist |
| Recursive alias / recursive type legality checks | Prevents pathological or runtime-hostile type cycles | Current alias resolution is strong, but legality policy is still shallow |
| Public API/interface drift diagnostics | Detect source modules, rendered interfaces, and native exports that no longer agree | Important for library modules and LSP/builtin extraction |
| Ambiguous module-name diagnostics | Prevent bad resolution when multiple module roots expose the same name | Especially relevant with `.starbmodpath` |
| Compile-time constant enforcement where required | Needed once more constant-only contexts are added | Keep this in sema, not lint |
| Stronger secure/catch propagation diagnostics | Warn/error when throwable or optional values are partially handled and partially leaked | Completes the current secure-decl model |

### Priority 2: future-dependent semantic checks

These should land when the corresponding language features stabilize:

| Feature | Dependency |
| --- | --- |
| Generic constraints / `where` clause enforcement | Generic constraints syntax and representation |
| Variance legality checks | Variance syntax and assignability policy |
| Enum exhaustiveness | Full enum semantics plus exhaustive branching form |
| Purity / async misuse diagnostics | Stable purity/async contract model |
| Ref/alias escape diagnostics | Final ADT/reference mutability semantics |

## Proposed Lint Engine Completion Features

### Required foundation work first

| Feature | Why it matters |
| --- | --- |
| AST-backed lint traversal | Removes fragile string matching and enables node-accurate findings |
| Semantic-fact input from compiler | Needed for type-aware, scope-aware, and import-aware linting |
| Rule-level suppression model | Required before the rule set grows materially |
| Stable safe/unsafe autofix classification | Needed so code actions stay trustworthy |
| Incremental workspace caching | Important if lint expands to whole-project semantic checks |

### Priority 0: semantic-aware lint rules

| Feature | Why it matters | Boundary |
| --- | --- | --- |
| Unused symbol lint with severity by symbol kind | Strong signal for cleanup and dead code | Can reuse sema reachability/usage data |
| Ignored non-`Void` results | Generalizes the current invocation warning into a configurable lint rule | Lint |
| Redundant cast / redundant `is` / redundant optional unwrap patterns | Flags noise and logic that no longer does anything | Lint |
| Constant-condition lint | Complements sema by catching suspicious but technically legal constant branches | Lint unless provably invalid |
| Duplicate branch predicates / equivalent `if` arms | High-value bug detection | Lint |
| Empty control blocks and empty catch bodies | Good safety/correctness signal | Lint |
| Deprecation usage lint | Can be routed through lint even if declaration-side parsing stays in sema | Shared with sema diagnostics |
| Shadowing lint with policy knobs | Some teams want warnings, some want errors only for dangerous cases | Lint configuration layer |

### Priority 1: performance and safety lint

| Feature | Why it matters |
| --- | --- |
| Repeated regex construction in loops | Real cost, now that regex is a builtin API |
| String concatenation in loops | Common accidental quadratic pattern |
| Repeated container size/property lookups in hot loops | Actionable micro-optimization hint |
| Repeated module/native calls in loops when hoistable | Useful once module use expands |
| Boxed numeric churn / repeated numeric casts | Relevant to current runtime optimization work |
| Avoidable collection copies / temporary arrays | Helps users write runtime-friendly code |
| Broad catch that swallows errors | Strong safety signal |
| Unchecked optional/throwable results | Fits Starbytes' optional/throwable model well |
| Unsafe cast hot-spots | Good warning for runtime failure-prone code |

### Priority 2: API, docs, and maintainability lint

| Feature | Why it matters |
| --- | --- |
| Public declaration docs completeness | Current comment rule is too shallow for exported APIs |
| Param/return doc coverage and drift | Keeps interfaces and hovers trustworthy |
| Naming convention rules by symbol kind | Useful once the symbol table is available to lint |
| Import ordering / duplicate import / unused import organization | Better project hygiene with scoped module imports |
| Public API stability lint for library modules | Helps `starbpkg` and interface generation workflows |

## Recommended Delivery Order

1. Keep lint on compiler AST plus semantic facts.
2. Add unused/shadowing/deprecation checks with shared symbol-usage data.
3. Add constant-condition and dead-store analysis.
4. Expand lint into performance/safety/API hygiene rules once the semantic substrate is stable.

## Done State

Both engines are close to complete when the following are true:

- invalid programs are rejected with flow-aware semantic diagnostics, not just local expression checks
- suspicious but legal programs are surfaced by lint using symbol/type/control-flow facts instead of text heuristics
- public API drift across source, interface, native modules, and documentation is diagnosable
- the LSP and `starbytes-ling` consume the same semantic truth instead of rebuilding approximations

## Practical Recommendation

Do not spend the next cycle adding many more line-based lint rules. The higher-return move is:

1. keep exposing compiler AST + semantic facts to lint
2. build unused/deprecation/import-ownership rules on that shared substrate
3. expand CFG-backed semantic-aware lint beyond the remaining shallow rules

That path completes both engines faster than growing either one in isolation.
