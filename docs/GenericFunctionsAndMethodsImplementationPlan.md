# Generic Free Functions And Methods Implementation Plan

## Goal

Add generic free functions and generic methods to Starbytes without destabilizing the current compiler/runtime pipeline.

## Recommended Strategy

Use explicit type-argument specialization first, with backend-aware code generation.

Rationale:

- Starbytes already models concrete runtime function/class identities.
- The current semantic pipeline is centered on concrete `ASTType` substitution.
- The current backend can execute generic free-function bodies after semantic specialization without cloning a separate emitted symbol per specialization.
- An erased or dictionary-passing design would require broader runtime and dispatch changes.

## Syntax Targets

Phase-complete target syntax:

```starbytes
func identity<T>(value:T) T {
    return value
}

class Box<T> {
    func pair<U>(left:T, right:U) Map<String,Any> {
        return {"left": left, "right": right}
    }
}

decl a = identity<Int>(4)
decl b = box.pair<String>(1, "x")
```

## Design Principles

1. Implement generic free functions before generic methods.
2. Require explicit type arguments before adding inference.
3. Specialize only used call sites, and emit dedicated symbols only when the backend requires them.
4. Keep unspecialized generic function values unsupported until specialization semantics are in place.

## Phase Plan

### Phase 1: Generic Free Function Declarations

Scope:

- parse generic parameter lists on free functions
- represent generic params in AST
- allow generic params in free-function parameter types, return types, and function bodies
- preserve generic params in AST dump, interface rendering, and symbol/signature presentation
- reject generic invocation/specialization for now

Not included:

- generic function calls
- generic methods
- type argument inference
- monomorphized code generation

Deliverables:

- `func identity<T>(value:T) T { ... }` parses successfully
- `starbytes check` accepts generic free-function declarations
- generic params survive AST/interface/LSP signature rendering
- invocation of generic free functions is explicitly rejected

### Phase 2: Generic Free Function Invocation Syntax

Scope:

- parse `name<T>(...)`
- parse `Scope.name<T>(...)`

Deliverables:

- AST invocation nodes carry explicit type arguments for non-constructor calls

### Phase 3: Semantic Specialization For Generic Free Functions

Scope:

- validate explicit type arg count
- validate explicit type arg existence
- substitute bindings into param/return types

Deliverables:

- `identity<Int>(4)` type-checks

### Phase 4: Monomorphized Code Generation For Generic Free Functions

Scope:

- ensure runtime execution remains correct after explicit semantic specialization
- introduce dedicated emitted specializations only if the backend requires them later
- keep unused specializations from forcing extra codegen work

Deliverables:

- generic free functions run with explicit specialization
- plan records that the current backend does not need separate cloned symbols for this phase

### Phase 5: Generic Methods

Scope:

- parse generic method parameter lists
- bind receiver generic params plus method generic params
- support `obj.method<T>(...)`
- reject generic interface methods until Phase 6 lands

Deliverables:

- generic methods on concrete receiver types type-check and run
- generic interface methods fail with an explicit deferred-feature diagnostic

### Phase 6: Generic Interface Methods

Scope:

- define override/implementation compatibility for generic methods on interfaces

Deliverables:

- interface-dispatched generic methods are semantically stable
- implementing class methods must match interface generic method arity and signature structurally

### Phase 7: Type Argument Inference

Scope:

- infer generic args from call arguments
- later infer from contextual expected type when safe

Deliverables:

- `identity(4)` can infer `T = Int`
- uninferable or conflicting calls fail with dedicated generic-inference diagnostics

## Compiler Work Partition

1. AST/parser
2. declaration-time semantics
3. invocation parser
4. invocation semantics
5. backend-specific specialization/codegen
6. methods
7. interface methods
8. inference

## Current Status

- Phase 1: implemented
- Phase 2: implemented
- Phase 3: implemented
- Phase 4: implemented for the current backend through semantic specialization and runtime execution without cloned emitted symbols
- Phase 5: implemented for class methods with explicit type arguments
- Phase 6: implemented for explicit generic interface methods and implementation compatibility checks
- Phase 7: implemented for inference from invocation arguments; contextual expected-type inference remains deferred

## Phase 1 Notes

Phase 1 is intentionally declaration-only.

That keeps the implementation thin while still adding real user-visible progress:

- syntax exists
- semantic validation exists for declarations
- documentation/signatures preserve the generic params

Call support comes later because it requires invocation syntax, specialization rules, and code generation.
