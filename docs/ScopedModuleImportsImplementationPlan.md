# Scoped Module Imports Implementation Plan

## Goal

Make `import ModuleName` expose a namespace-style scope so module members are accessed as:

```starbytes
import Time
import Unicode

decl tz = Time.timezoneUTC()
decl form = Unicode.NFC
```

The long-term target is to stop flattening imported module symbols into file scope.

## Status

- Slice 1, 2, and 3 are implemented.
- `import Time` binds `Time` as a namespace-style scope for public members.
- Unqualified imported functions/constants are rejected.
- The enforced form is `ModuleName.member`.

Current examples:

```starbytes
import Time
import Unicode

decl tz = Time.timezoneUTC()
decl form = Unicode.NFC
```

## Design

Imported modules should be surfaced through the same scope-member path already used by:

- `scope Core { ... }`
- `Core.answer()`
- `Mode.DEBUG`

The clean model is:

- each `import ModuleName` binds `ModuleName` as a scope symbol
- public module members are resolved under that scope
- unqualified imported names are removed from file-scope lookup

## Implemented Rollout

### Slice 1: Additive Namespace Overlay

Delivered the initial namespace overlay.

Behavior delivered in the first slice:

- `Time.timezoneUTC()` works
- `Unicode.NFC` works

Implementation:

- keep the original imported symbol table available as imported backing state
- synthesize a second symbol-table view per imported module:
  - create a synthetic namespace scope for the module name
  - create a global scope entry named after the module
  - mirror top-level exported entries into the synthetic module scope
- member access then resolves through existing `Entry::Scope` logic in `ExprSema.cpp`

Why this slice:

- minimal risk
- validates namespace semantics end-to-end
- avoids a repo-wide migration in one step

### Slice 2: Broaden Surface

Completed after slice 1 stabilization:

- qualify stdlib and source-module call sites in tests/docs
- ensure nested exported scopes behave correctly through `Module.Sub.member`
- extend tooling expectations where needed

### Slice 3: Breaking Cleanup

Completed after qualified imports were adopted:

- remove flat imported symbol injection from unqualified lookup
- require `ModuleName.member`
- keep collisions explicit and predictable

## Compiler Touchpoints

- `tools/driver/main.cpp`
  - attaches imported backing tables and namespace overlays during module loading
- `include/starbytes/compiler/SymTable.h`
  - separates imported backing tables from ordinary lexical overlay tables
- `src/compiler/SymTable.cpp`
  - resolves namespaced import overlays and imported backing lookups
- `src/compiler/ExprSema.cpp`
  - reuses the existing scope-member resolution path and rejects unqualified imported globals

## Final Acceptance

The implemented import model is complete when:

1. `import Time` allows `Time.timezoneUTC()`
2. `import Unicode` allows `Unicode.NFC` and `Unicode.localeCurrent()`
3. `import Util` allows `Util.inc(41)` for source modules
4. unqualified imported members are rejected
5. regression tests cover stdlib, source modules, and negative cases

## Deferred

Not included in the current implementation:

- import alias syntax
- qualified type references like `Time.Duration`
- LSP-specific import namespace presentation changes
