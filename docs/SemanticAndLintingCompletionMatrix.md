# Semantic And Linting Completion Matrix

Last updated: March 12, 2026

## Purpose

This document re-evaluates Starbytes semantic analysis and linting against two baselines:

1. the current Starbytes compiler and linguistics implementation
2. the capability families used by Clang Sema, Clang diagnostics, clang-tidy, and the Clang Static Analyzer

The goal is not to copy C or C++ behavior mechanically. The goal is to make the Starbytes semantic and linting surface complete for the language that exists today, while also making the boundary between compiler errors, lint warnings, and future analyzer-grade checks explicit.

## External Reference Model

The capability families in this matrix were cross-checked against these LLVM/Clang references:

- Clang Sema API: [clang::Sema](https://clang.llvm.org/doxygen/classclang_1_1Sema.html)
- Clang diagnostics model: [Expressive Diagnostics](https://clang.llvm.org/diagnostics)
- Clang warning taxonomy: [DiagnosticsReference](https://clang.llvm.org/docs/DiagnosticsReference.html)
- Warning suppression model: [WarningSuppressionMappings](https://clang.llvm.org/docs/WarningSuppressionMappings.html)
- clang-tidy architecture and check families: [Clang-Tidy](https://clang.llvm.org/extra/clang-tidy/)
- path-sensitive analyzer families: [Clang Static Analyzer checkers](https://clang.llvm.org/docs/analyzer/checkers.html)

These references matter because they separate three things cleanly:

- semantic validity checks
- lint-style quality guidance
- deeper path-sensitive analyzer checks

Starbytes should adopt the same separation.

## Local Basis

This matrix is based on the current implementation in:

- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/LintEngine.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/SuggestionEngine.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/compiler/SemanticA.h`
- `/Users/alextopper/Documents/GitHub/starbytes-project/include/starbytes/linguistics/LintEngine.h`

## Boundary

### Semantic analyzer owns

- anything that makes a program invalid
- exact type, symbol, module, and runtime contract enforcement
- flow analysis required for correctness and soundness
- declaration and attribute legality
- diagnostics that code generation and runtime safety depend on

### Lint engine owns

- suspicious but legal constructs
- maintainability, readability, and style policy
- performance hints and API hygiene
- optional team policy checks and configurable severity

### Analyzer-tier pass owns

- expensive path-sensitive and interprocedural reasoning
- symbolic execution style checks
- resource and protocol misuse patterns that exceed normal sema cost budgets

Starbytes does not have this third tier yet. The matrix includes it so the roadmap stays honest.

## Current State Summary

- Semantic analysis is already strong on declaration legality, type checking, generics, modules, secure declarations, native declaration validation, inheritance/interface conformance, definite assignment, unused binding warnings, and deprecation diagnostics.
- Linting is now AST-backed for its real rules, but the rule surface is still small.
- The main remaining compiler gaps are not expression typing. They are flow refinement, override/API contract checks, constant-condition reasoning, drift checks, and diagnostics policy/configuration.
- The main remaining lint gaps are not plumbing. They are rule breadth, semantic-fact usage, suppression policy, and workspace-scale behavior.

## Current Semantic Analyzer Coverage

| Capability family | Current status | Notes |
| --- | --- | --- |
| Name lookup and lexical scoping | Implemented | Lexical lookup, class member lookup, scope lookup, imported module namespace lookup, undefined symbol diagnostics |
| Import and module namespace rules | Implemented | Imported module members must be referenced as `ModuleName.member`; unused import warnings exist |
| Declaration legality | Implemented | Invalid declaration placement, missing bodies, invalid native decl shapes, const initialization, secure decl legality |
| Core expression typing | Implemented | Arithmetic, logic, bitwise, comparison, ternary, casts, indexing, assignment, `await`, optional/throwable compatibility |
| Collection typing | Implemented | Typed arrays/maps/tasks and collection shape matching |
| Call checking | Implemented | Arity, argument types, return compatibility, generic specialization and inference |
| Generic declarations and invocations | Implemented for current feature set | Generic types, free functions, methods, interface methods, defaults, constructor generics |
| Class/interface conformance | Implemented | Single superclass, interface conformance, duplicate methods, duplicate constructor arity, circular inheritance rejection |
| Definite assignment | Implemented | Locals, params, secure catch bindings, top-level/module flow |
| Must-return analysis | Implemented | Non-`Void` functions, methods, inline functions, interface default bodies |
| Unused bindings | Implemented | Imports, locals, params, `@private` fields |
| Deprecation diagnostics | Implemented | Variables, functions, classes, interfaces, type aliases, fields, methods; payload surfaces at use sites |
| Unreachable code | Partial | Only direct proven-unreachable after `return`-style flow in current block walker |
| Optional/throwable refinement | Partial | Secure-decl semantics exist, but full flow-sensitive narrowing does not |
| Override contracts | Missing | No explicit override-required, invalid-override, accidental-overload diagnostics |
| Constant-condition reasoning | Missing | No tautology/impossible-branch diagnostics |
| API drift checks | Missing | No source/interface/native consistency audit |
| Diagnostic policy/configuration | Partial | Diagnostics exist, but groups, fine-grained suppression, and warning policy are incomplete |

## Current Lint Engine Coverage

### Implemented engine capabilities

- compiler-backed AST analysis input
- rule registry with stable IDs and categories
- severity selection and `strict` escalation
- enable/disable by selectors
- max findings cap
- suggestion and code-action plumbing

### Implemented rules

| Rule | Code | Category | Status |
| --- | --- | --- | --- |
| `style.trailing_whitespace` | `SB-LINT-STYLE-S0001` | Style | Implemented |
| `correctness.assignment_in_condition` | `SB-LINT-CORR-C0001` | Correctness | Implemented |
| `performance.new_in_loop` | `SB-LINT-PERF-P0001` | Performance | Implemented |
| `safety.untyped_catch` | `SB-LINT-SAFE-A0001` | Safety | Implemented |
| `docs.missing_decl_comment` | `SB-LINT-DOC-D0001` | Docs | Implemented |
| `correctness.shadowing` | `SB-LINT-CORR-C0004` | Correctness | Implemented |

### Missing engine capabilities

- per-rule suppression in source and config
- semantic symbol identity as a first-class lint input
- compiler-warning ingestion as lint categories or mirrors
- workspace/project aggregation for cross-file lint
- safe-vs-unsafe autofix classification hardening
- per-file or path-scoped warning policy

## Complete Semantic Analyzer Capability Checklist

Status meanings:

- `Implemented`: present and materially usable now
- `Partial`: present in a narrower form than a complete language-quality implementation
- `Missing`: should exist for the current language, but does not
- `Future feature dependent`: wait until the language feature is finalized
- `Analyzer-tier later`: valuable, but should not live in ordinary sema

### 1. Name lookup, redeclaration, and scope legality

| Check family | Status | Notes |
| --- | --- | --- |
| Undefined symbol diagnostics | Implemented | Present |
| Undefined member diagnostics | Implemented | Present |
| Duplicate symbol in scope | Implemented | Present for major decl kinds |
| Qualified import enforcement | Implemented | Present |
| Ambiguous symbol lookup diagnostics | Implemented | Same-scope duplicates and cross-module imported-name ambiguity now diagnose instead of silently choosing a first match |
| Shadowing diagnostics as compiler warnings | Implemented | Local/catch shadowing of outer bindings now warns in sema; lint rule remains for analysis tooling |
| Import cycle diagnostics with clear path reporting | Implemented | Cycle reports now include the module chain and the import file:line for each edge in the loop |
| Wrong-namespace or wrong-scope declaration diagnostics | Implemented | Namespace-only declarations now diagnose consistently in function and block scopes, including nested declarations inside control-flow blocks |
| Forward declaration and declaration-ownership rules | Future feature dependent | Depends on whether Starbytes adds true forward declarations |

### 2. Declaration, initialization, and object-state legality

| Check family | Status | Notes |
| --- | --- | --- |
| Const/immutable initialization requirements | Implemented | Present |
| Use-before-initialization | Implemented | Present for current flow model |
| Must-return on all paths | Implemented | Present |
| Unused imports/locals/params/private fields | Implemented | Present as compiler warnings |
| Constructor body return rejection | Implemented | Present |
| Field definite initialization policy | Implemented | Required own-fields without declaration initializers must be definitely assigned on every constructor exit path; classes/structs with required fields and no constructors now diagnose |
| Duplicate field initialization / double-initialize diagnostics | Implemented | Present as compiler warning for repeated `self.field = ...` writes along the same constructor path |
| Partially initialized object diagnostics | Implemented | Present as compiler warnings for required-field reads, `self` escape, and `self` method calls before all required fields are initialized |
| Dead-store diagnostics | Implemented | Present as compiler warnings for local/parameter writes that are overwritten or reach scope exit unread in the current conservative control-flow model |

### 3. Type formation and type-system legality

| Check family | Status | Notes |
| --- | --- | --- |
| Unknown type diagnostics | Implemented | Present |
| Generic argument count and defaults | Implemented | Present |
| Generic type existence and substitution | Implemented | Present |
| Function type legality | Implemented | Present |
| Typed collection legality | Implemented | Present |
| Recursive alias legality policy | Partial | Alias resolution exists; legality against hostile cycles is not fully formalized |
| Invalid self-referential type graphs | Missing | Needed for stronger soundness and better diagnostics |
| Constraint / where-clause enforcement | Future feature dependent | Feature not implemented yet |
| Variance legality | Future feature dependent | Feature not implemented yet |
| Constant-type-only contexts | Future feature dependent | Depends on more compile-time-only contexts |

### 4. Conversions, casts, and numeric diagnostics

| Check family | Status | Notes |
| --- | --- | --- |
| Invalid cast diagnostics | Implemented | Present |
| Optional/throwable compatibility | Implemented | Present |
| Numeric operator typing | Implemented | Present |
| Runtime `is` legality | Implemented | Present |
| Lossy narrowing warnings | Missing | Clang has many numeric conversion warnings; Starbytes has none yet |
| Suspicious mixed numeric type warnings | Missing | Especially useful with `Int`, `Long`, `Float`, `Double` |
| Redundant cast diagnostics | Missing | Better as lint unless it changes validity |
| Impossible runtime-type-test diagnostics | Partial | Present for exact builtin/runtime-exact `is` cases; general class-hierarchy reasoning remains conservative |

### 5. Calls, overload-like behavior, and callable contracts

| Check family | Status | Notes |
| --- | --- | --- |
| Arity and argument type checking | Implemented | Present |
| Callable return compatibility | Implemented | Present |
| Generic callable inference | Implemented | Present for current model |
| Ignored non-`Void` results | Partial | Compiler warns in current direct form; could be generalized and policy-controlled |
| Override-required / invalid-override / accidental-overload diagnostics | Partial | Superclass override contracts now diagnose invalid member-kind override plus generic-count, parameter-signature, and return-type mismatches; explicit override-required still awaits an override marker feature |
| Covariant/contravariant override compatibility diagnostics | Missing | Needed once override contracts are formalized |
| Virtual/interface dispatch contract diagnostics | Partial | Interface conformance exists; richer override semantics do not |
| Recursion diagnostics such as obvious infinite recursion | Missing | Clang has related warning families; Starbytes has none |

### 6. Control flow, dataflow, and refinement

| Check family | Status | Notes |
| --- | --- | --- |
| Straight-line definite assignment | Implemented | Present |
| Branch merge analysis | Implemented | Present in current flow walker |
| Loop-aware conservative flow | Partial | Present conservatively, but not rich |
| Unreachable code detection | Partial | Limited |
| Constant-condition and impossible-branch detection | Implemented | Compiler warnings now cover constant Bool conditions in conditionals/loops/ternaries, with conservative impossible-`is` branch diagnostics |
| Flow-sensitive optional/throwable narrowing | Missing | Important for `secure`-heavy code |
| Flow-sensitive type narrowing after `is` | Missing | Useful once `is` becomes a real refinement tool |
| Exhaustiveness diagnostics | Future feature dependent | Needs stable enum/pattern branching form |
| Switch-like fallthrough diagnostics | Future feature dependent | Depends on language constructs |
| Path-sensitive null/optional misuse | Analyzer-tier later | Better in a dedicated analyzer pass |

### 7. Object model, inheritance, and interface contracts

| Check family | Status | Notes |
| --- | --- | --- |
| Circular inheritance rejection | Implemented | Present |
| Single-superclass enforcement | Implemented | Present |
| Interface method compatibility | Implemented | Present |
| Generic interface method compatibility | Implemented | Present |
| Duplicate method and ctor-arity rejection | Implemented | Present |
| Missing required method diagnostics with rich mismatch notes | Partial | Present, but note quality and mismatch detail can be deeper |
| Private/protected-style access diagnostics | Partial | `@protected` access is enforced for class/interface methods and fields; `@private` remains a warning-oriented field annotation rather than enforced access control |
| Forbidden subclass / sealed type diagnostics | Future feature dependent | Depends on future attribute or type contract surface |
| Constructor-state protocol diagnostics | Missing | Useful once richer init rules or field requirements stabilize |

### 8. Modules, native interfaces, and API drift

| Check family | Status | Notes |
| --- | --- | --- |
| Import placement and syntax legality | Implemented | Present |
| Module namespace access rules | Implemented | Present |
| Native declaration shape validation | Implemented | Present |
| Native/source/interface symbol mismatch detection | Missing | Important for stdlib and library modules |
| Rendered interface drift diagnostics | Missing | Source vs `.starbint` mismatch is not audited |
| Ambiguous module root resolution diagnostics | Missing | Important with `.starbmodpath` |
| Duplicate module name collision diagnostics | Missing | Needed for larger workspaces |
| ABI contract diagnostics for native modules | Missing | Important for runtime stability |

### 9. Attributes, annotations, and API lifecycle

| Check family | Status | Notes |
| --- | --- | --- |
| Attribute legality validation | Implemented | Present for current attributes |
| Deprecation use warnings | Implemented | Present with payloads |
| Readonly/private/native/deprecated declaration-shape validation | Implemented | Present |
| Attribute argument validation richness | Partial | Current checks are good, but attribute schema is not generalized |
| Availability / version / platform annotations | Missing | Useful once stdlib and package ecosystem grow |
| API evolution annotations beyond deprecation | Missing | Examples: renamed, unavailable, experimental |

### 10. Diagnostics quality and operability

Clang’s diagnostics model is a useful standard here: accurate columns, range highlights, fix-its, preserved user spelling, notes, and good recovery all materially improve compiler usability.

| Check family | Status | Notes |
| --- | --- | --- |
| Precise source ranges and columns | Implemented | Present |
| Related notes and fix-its | Partial | Infrastructure exists; coverage is uneven |
| Type spelling preservation in diagnostics | Partial | Present in places, but not consistently polished |
| Recovery after local semantic failures | Partial | Good enough for many cases, but not systematized |
| Fine-grained diagnostic groups/codes | Partial | Codes exist, but grouping is still broad |
| Warning-as-error policy | Missing | Important for toolchain maturity |
| Per-file/path warning suppression | Missing | Clang-style suppression mapping analogue does not exist |
| Machine-readable structured diagnostics parity | Partial | LSP/CLI structured output exists, but policy surface is incomplete |

## Complete Lint Capability Checklist

The lint surface should be modeled after clang-tidy families, but adapted to Starbytes instead of copying C++-specific checks.

### A. Foundation and policy

| Capability | Status | Notes |
| --- | --- | --- |
| Compiler AST-backed traversal | Implemented | Present |
| Compiler semantic facts as lint input | Partial | Some shared facts exist, but symbol identity/type/control-flow use is still shallow |
| Rule registry and stable IDs | Implemented | Present |
| Rule enable/disable selectors | Implemented | Present |
| Source-level suppression comments | Missing | Clang-tidy-style suppression analogue is not present |
| Per-path or per-module suppression policy | Missing | Important for adoption in large repos |
| Warning severity remapping by rule | Partial | `strict` exists, but richer per-rule policy is missing |
| Warnings-as-errors for lint | Missing | Desirable for CI |
| Safe-vs-unsafe fix classification | Partial | Plumbing exists, policy is not hardened |
| Workspace/project-wide lint aggregation | Missing | Current model is mostly file-local |

### B. Correctness and bug-prone lint

| Capability | Status | Notes |
| --- | --- | --- |
| Assignment in condition | Implemented | Present |
| Shadowing policy lint | Implemented | Present |
| Ignored non-`Void` results | Missing | Good fit for lint policy layer |
| Dead stores | Missing | High-value bugprone rule |
| Duplicate conditions or identical branches | Missing | High-value logic check |
| Constant conditions that are suspicious but legal | Missing | Good bugprone rule |
| Empty branches / empty catches / empty loops | Missing | Good signal |
| Redundant cast / redundant `is` / redundant unwrap | Missing | Good cleanup and bugprone signal |
| Suspicious self-assignment / no-op update | Missing | Clang-family analogue worth adding |
| Suspicious compare against impossible values | Missing | Useful once numeric warnings land |

### C. Performance lint

| Capability | Status | Notes |
| --- | --- | --- |
| Allocation inside loop | Implemented | Present |
| Repeated regex construction | Missing | Strong fit now that `Regex` is builtin |
| String concatenation in loops | Missing | Common quadratic pattern |
| Avoidable collection copies | Missing | Good with array/map-heavy code |
| Repeated module/native calls in loops | Missing | Good hoisting hint |
| Repeated size/property lookup in loops | Missing | Good micro-optimization hint |
| Boxing/cast churn | Missing | Relevant to current runtime optimization work |
| Inefficient numeric patterns | Missing | Useful once numeric runtime specialization expands |

### D. Safety and security lint

| Capability | Status | Notes |
| --- | --- | --- |
| Untyped catch | Implemented | Present |
| Broad catch that swallows errors | Missing | Strong safety rule |
| Unchecked optional/throwable results | Missing | Natural Starbytes-specific safety rule |
| Unsafe cast hotspots | Missing | High-value runtime safety lint |
| Dangerous native/module API misuse patterns | Missing | Important as stdlib grows |
| API protocol misuse | Missing | Better after more module surface stabilizes |

### E. Readability, style, and maintainability lint

| Capability | Status | Notes |
| --- | --- | --- |
| Trailing whitespace | Implemented | Present |
| Naming conventions by symbol kind | Missing | Useful once symbol identity is passed through lint |
| Import ordering and duplicate imports | Missing | Good hygiene rule |
| Overly complex expressions / deep nesting | Missing | Good readability family |
| Prefer clearer scoped access or API shape | Missing | Useful once style policy is formalized |
| Redundant qualifiers or noise syntax | Missing | Good cleanup family |

### F. Documentation and API hygiene lint

| Capability | Status | Notes |
| --- | --- | --- |
| Missing top-level declaration comments | Implemented | Present |
| Param/return docs completeness | Missing | Needed for public API quality |
| Doc drift against signature | Missing | High-value and LSP-visible |
| Public API export docs coverage | Missing | Good library-facing rule |
| Deprecation surface hygiene | Missing | Examples: missing deprecation message, stale replacement text |

### G. Portability, configuration, and ecosystem lint

Starbytes does not have C/C++ portability problems, but it does have ecosystem portability issues that fill a similar role.

| Capability | Status | Notes |
| --- | --- | --- |
| Module-path portability warnings | Missing | `.starbmodpath` and workspace assumptions |
| Native module availability warnings | Missing | Useful when code depends on optional native deps |
| Stdlib/platform behavior compatibility hints | Missing | Relevant for ICU/OpenSSL/curl-backed modules |
| Versioned API usage or package-policy lint | Missing | Important once `starbpkg` package ecosystem expands |

## Analyzer-Tier Checks That Should Not Be Forced Into Ordinary Sema

These are inspired by Clang Static Analyzer families. They are valuable, but they belong in a future deeper analysis pass, not in ordinary per-file semantic analysis.

| Capability | Why it matters |
| --- | --- |
| Path-sensitive optional/throwable misuse | Requires symbolic path reasoning |
| Resource leak and protocol misuse | Better as interprocedural analysis |
| Null/absence dereference family beyond local checks | Better with path-sensitive state |
| Cross-function dead-store and unused-return propagation | Requires call graph reasoning |
| Stateful API protocol checks for IO/FS/Net/Crypto | Better as checker families |
| Dataflow-derived impossible state checks | Too expensive for ordinary sema |

## Re-evaluated Priority Order

### Priority 0: finish semantic completeness for the current language

1. constant-condition and impossible-branch diagnostics
2. override contract diagnostics
3. stronger optional/throwable flow refinement
4. field/class initialization policy diagnostics
5. ambiguous module name and module drift diagnostics
6. warning policy surface: warning groups, warnings-as-errors, suppression model

### Priority 1: finish lint as a real semantic-aware tool

1. suppression model and per-rule severity remapping
2. ignored-result, dead-store, duplicate-branch, empty-block, redundant-cast rules
3. doc completeness and doc drift rules
4. import organization and naming convention rules
5. performance rules for regex, string, module calls, and collection copies

### Priority 2: add analyzer-tier checks as a separate pass

1. path-sensitive optional/throwable analysis
2. resource/protocol misuse families
3. cross-file API misuse and flow checks

## Practical Recommendation

If the goal is a complete language-quality front end, the next semantic work should focus on compiler-owned correctness gaps, not more heuristic lint rules.

If the goal is a complete developer experience, the next lint work should focus on:

1. suppression and policy configuration
2. bugprone and dead-store style rules that reuse compiler facts
3. documentation and API hygiene rules that directly improve LSP and library quality

## Done State

The semantic and linting stack is close to complete when all of the following are true:

- invalid programs are rejected with flow-aware, symbol-aware, type-aware diagnostics
- legal but suspicious programs are surfaced by lint through compiler facts, not source heuristics
- diagnostics are configurable by group and suppressible at practical granularity
- source modules, generated interfaces, native exports, LSP data, and docs can be checked for drift
- analyzer-grade checks are separated into their own tier instead of bloating ordinary sema
