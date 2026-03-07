# Generics Expansion Matrix

## Scope

This document proposes the next expansion wave for the Starbytes generic system beyond the currently implemented baseline:

- generic aliases
- generic classes / structs / interfaces
- generic free functions
- generic methods
- generic interface methods
- invocation-time specialization
- inference from invocation arguments

## Current Baseline

Current compiler shape that matters for the remaining work:

- generic parameters are still modeled as `std::vector<ASTIdentifier *>`
- constructors are separate `ASTConstructorDecl` nodes with no generic parameter model
- enums are still lowered through `SCOPE_DECL`-style parsing rather than a dedicated enum AST/type model
- scopes are namespace-like containers, not generic templates
- modules are file/import/cache units, not first-class AST declarations
- type matching is mostly invariant and exact, with a few builtin exceptions

That means five requested features do not fit cleanly into the current generic parameter representation. The first required step is a data-model upgrade.

## Decision Matrix

| Feature | Current State | Value | Complexity | Main Blockers | Recommendation | Target Wave |
| --- | --- | --- | --- | --- | --- | --- |
| Generic constructors as separate declarations | Not supported | High | Medium | `ASTConstructorDecl` has no generic params; constructor call syntax only carries class type args | Implement | Wave 2 |
| Generic enums | Effectively unsupported | Medium | High | enums are parsed as scope-like constant containers, not real generic types | Implement only after real enum AST/type model exists | Wave 4 |
| Generic scopes | Unsupported | Low-Medium | Very High | scopes are namespaces; no instantiation model, no scoped generic cache/interface model | Defer; treat as namespace-template feature, not core generics | Wave 6 |
| Generic modules | Unsupported | Low-Medium | Very High | modules are import/cache units; would need parameterized `.starbint` and `.starbmod` keys | Defer after generic scopes prove necessary | Wave 7 |
| Constraints / where clauses / trait bounds | Unsupported | Very High | High | no generic param metadata beyond names; no first-class bounds storage or constraint solver | Implement | Wave 3 |
| Variance annotations | Unsupported | Medium-High | Very High | current assignability is largely invariant/exact; no general subtype relation engine | Defer until after constraints and type-relation rewrite | Wave 5 |
| Defaults for generic parameters | Unsupported | Medium | Medium | generic params are identifiers only; arity checking assumes exact counts | Implement early | Wave 1 |

## Foundational Matrix

| Foundation Change | Why It Is Needed | Affects |
| --- | --- | --- |
| Replace identifier-only generic params with `ASTGenericParamDecl` | defaults, bounds, variance, and constructor generics all need metadata per generic parameter | parser, AST, semantic analysis, interface generation, LSP |
| Add symbol-table support for rich generic parameter metadata | semantic checks must see defaults, bounds, and variance; interface rendering must preserve them | `SymTable`, `SemanticA`, interface gen |
| Add reusable generic argument resolution pipeline | explicit args, defaults, and inference should resolve through one code path | `ExprSema`, constructor call resolution |
| Separate real enum type modeling from scope lowering | generic enums are not meaningful on top of namespace constants alone | parser, AST, semantics, codegen/runtime metadata |
| Build a real subtype/assignability relation | variance and constraint checking both need more than exact type equality | `ASTType::match`, semantic type utilities |

## Recommended Syntax

### Generic Constructors

Declaration:

```starbytes
class Box<T> {
    new<U>(seed:U) {
        self.value = seed
    }
}
```

Recommendation for call syntax:

- primary: infer constructor generic args from invocation arguments
- optional later: `new Box<Int><String>("seed")`

Reasoning:

- the language already supports inference for generic calls
- requiring a second generic argument list on constructor calls raises parser complexity immediately
- inference-first keeps constructor syntax closer to existing `new Type<T>(...)`

### Constraints / Where Clauses / Trait Bounds

Inline bound:

```starbytes
func max<T: Comparable>(a:T, b:T) T
```

Trailing where clause:

```starbytes
func merge<K,V>(lhs:Map<K,V>, rhs:Map<K,V>) Map<K,V>
where K: Hashable, V: Serializable
```

Recommendation:

- support both inline bounds and trailing `where`
- lower inline bounds into the same internal bound list used by `where`
- treat bounds as compile-time conformance checks against interfaces / builtin traits only

### Variance

```starbytes
interface Producer<out T> { ... }
interface Consumer<in T> { ... }
class Box<T> { ... }        // invariant by default
```

Recommendation:

- allow variance only on class/interface/struct generic type declarations
- keep functions/method generic params invariant
- default all generics to invariant

### Defaults For Generic Parameters

```starbytes
class Box<T = Any> { ... }
func decode<T = String>(source:String) T
interface Cache<K: Hashable, V = Any> { ... }
```

Recommendation:

- only trailing generic params may have defaults
- defaults may reference earlier generic params, not later ones
- defaults must satisfy declared bounds

### Generic Enums

Recommended only if Starbytes moves enums beyond scope-lowered integer constants:

```starbytes
enum Option<T> {
    SOME(T)
    NONE
}
```

If enums remain simple constant namespaces, generic enums should not ship, because they add syntax without meaningful generic value.

### Generic Scopes

```starbytes
scope Math<T> {
    func clamp(v:T, lo:T, hi:T) T
}

decl x = Math<Int>.clamp(4, 0, 10)
```

Recommendation:

- treat this as a namespace template instantiation
- do not mix it into the normal module import feature until scope templates are stable

### Generic Modules

Conceptual syntax only:

```starbytes
import Collections<Int> as IntCollections
```

Recommendation:

- do not implement until generic scopes and parameterized interface caching are proven
- modules should be the last generic expansion item, not the first

## Implementation Plan

### Wave 0: Generic Parameter Data Model

Introduce a dedicated generic parameter node, for example:

```cpp
struct ASTGenericParamDecl {
    ASTIdentifier *id = nullptr;
    enum Variance { Invariant, In, Out } variance = Invariant;
    ASTType *defaultType = nullptr;
    std::vector<ASTType *> bounds;
};
```

Replace:

- `ASTFuncDecl::genericTypeParams`
- `ASTClassDecl::genericTypeParams`
- `ASTInterfaceDecl::genericTypeParams`
- `ASTTypeAliasDecl::genericTypeParams`

With:

- `std::vector<ASTGenericParamDecl *> genericParams`

Extend:

- `ASTConstructorDecl` with generic params
- symbol table generic metadata for functions, types, and constructors
- AST dump / interface generation / LSP hover/signature rendering

This is the mandatory precursor for defaults, bounds, variance, and generic constructors.

### Wave 1: Defaults For Generic Parameters

### Parser

- parse `T = SomeType` in generic parameter lists
- enforce trailing-default rule during parsing or early sema

### Semantic analysis

- validate default types with the current generic environment
- update generic arity checks from exact-count to:
  - explicit provided count <= declared count
  - all omitted params must have defaults or be inferable
- materialize omitted defaults before specialization/inference finalization

### Invocation / type references

- support omitted type args for:
  - type references `Box<>` only if empty angle syntax is allowed later
  - constructor type refs `new Box()`
  - free-function/method/interface-method invocations

### Tests

- default on type declarations
- default on free functions
- default plus inference interaction
- invalid non-trailing default
- invalid default violating bound

### Wave 2: Generic Constructors As Separate Declarations

### Parser / AST

- add generic params to `ASTConstructorDecl`
- parse `new<U>(...) { ... }` in class scope

### Semantics

- constructor generic environment = class generic params + constructor generic params
- support constructor return/field initialization type resolution under that merged environment
- add duplicate constructor signature checking using:
  - arity
  - generic arity
  - parameter types after normalization

### Invocation

- choose constructor by value-parameter arity first
- infer constructor generic params from arguments by default
- optional explicit constructor generic args can be added later if inference-first is insufficient

### Runtime / codegen

- current backend likely does not need cloned constructor symbols if specialization remains semantic-only
- keep constructor metadata rich enough to support cloned symbols later if needed

### Tests

- success with inferred constructor generic args
- constructor generic using class generic in param types
- conflicting constructor generic inference
- explicit class generic + inferred constructor generic

### Wave 3: Constraints / Where Clauses / Trait Bounds

### Type-system model

- treat bounds as nominal conformance requirements against:
  - interfaces
  - builtin marker traits (if introduced)

### Parser

- parse inline bounds `T: Comparable`
- parse trailing `where` clauses
- lower both into one bound list per generic param

### Semantics

- validate that constrained types exist
- validate that defaults satisfy bounds
- validate that inferred / explicit type args satisfy all bounds
- reuse class-interface conformance logic where possible rather than inventing a second rule engine

### Diagnostics

- unknown bound type
- non-interface bound where trait/interface is required
- type argument does not satisfy bound
- conflicting or redundant bound sets

### Tests

- explicit generic call satisfying bounds
- inferred generic call satisfying bounds
- invalid explicit type arg
- invalid inferred type arg
- defaults interacting with bounds

### Wave 4: Generic Enums

### Recommendation

Do not bolt generics onto the current enum lowering.

Instead:

1. introduce `ASTEnumDecl`
2. give enums real type identity in the symbol table
3. only then add generic params

### Why

Current enums are parsed as namespace-like constant collections. A generic enum without payloads is mostly cosmetic.

### Suggested minimum viable generic enum

- payload-bearing cases
- generic parameter references inside case payloads
- simple pattern of construction/matching or equivalent access semantics

If payload enums are not planned, generic enums should remain deferred.

### Wave 5: Variance Annotations

### Recommendation

Do not implement variance on top of the current mostly-invariant `ASTType::match` model.

First build a general assignability relation:

- exact match
- class inheritance
- interface conformance where applicable
- generic type constructor variance application

### Supported surface

- `out T`
- `in T`
- invariant default

### Enforcement

- validate declaration-site positions for unsafe use of `out` / `in`
- update assignment, parameter passing, return checking, collection nesting, and interface implementation checks

### Risk

This is the most cross-cutting feature in the list. It should ship only after:

- bounds exist
- type relation utilities are centralized
- regression coverage is broad

### Wave 6: Generic Scopes

### Model

Treat generic scopes as compile-time namespace templates.

That means:

- no runtime value identity
- no dynamic instantiation
- specialization happens in semantic/interface generation space

### Required changes

- add generic params to `ASTScopeDecl`
- allow `Scope<T>.member` lookup
- preserve specialized scope identities in interface output / symbol emitted names
- decide whether scope instantiation is:
  - purely semantic, or
  - materialized into emitted/specialized symbol namespaces

### Recommendation

Defer until the rest of the type-level generic system is complete. Most use cases are already covered by generic free functions, types, and nested scopes.

### Wave 7: Generic Modules

### Model

Parameterized modules are not just syntax. They change the import and cache model.

Required capabilities:

- parameterized module interface rendering
- cache key expansion for `.starbint` / `.starbmod`
- import cycle detection keyed by specialization
- stable emitted naming for instantiated module members
- package manager awareness for specialized cache cleanup

### Recommendation

Keep this out of the main generic expansion wave. Implement only if package authors genuinely need template-like module specialization that cannot be expressed with generic scopes + generic types.

## Recommended Delivery Order

1. Wave 0: generic parameter data model
2. Wave 1: defaults
3. Wave 2: generic constructors
4. Wave 3: constraints / bounds / where clauses
5. Wave 4: real enums, then generic enums if payload enums are accepted
6. Wave 5: variance
7. Wave 6: generic scopes
8. Wave 7: generic modules

## Practical Recommendation

The high-value path is:

1. rich generic parameter metadata
2. defaults
3. generic constructors
4. constraints

That delivers most of the practical language value without pulling module loading and namespace instantiation into the same release.

Generic scopes/modules and variance should be treated as separate, higher-risk projects rather than "finish-up" work.
