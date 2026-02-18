# Starbytes Language Semantics

Last updated: February 18, 2026

This document defines the runtime and static semantics implemented in the current Starbytes engine.

## 1. Evaluation Model

- Starbytes is interpreted from generated runtime bytecode.
- Evaluation is expression-oriented for value-producing nodes and statement-oriented for declarations/control flow.
- Semantic analysis is mandatory before execution; codegen assumes sema-clean AST.

## 2. Scope and Name Resolution

### 2.1 Scope Kinds

The compiler/runtime model distinguishes:

- namespace scope (`scope`)
- class scope
- function scope
- neutral block scopes (conditionals, loop blocks, secure catch blocks)

### 2.2 Resolution Order

Name lookup resolves in lexical scope chain and imported symbol tables. Namespace member access (`A.B.x`) is represented as member expressions with resolved scope metadata.

### 2.3 Emitted Symbol Names

Namespace-qualified symbols are mangled for runtime linkage using `__` separators:

- `Core.Math.answer` may emit as `Core__Math__answer`

## 3. Type System Semantics

### 3.1 Core Types

Built-ins: `Void`, `String`, `Bool`, `Array`, `Dict`, `Int`, `Float`, `Regex`, `Any`, `Task<T>`.

### 3.2 Matching Rules

Type matching behavior (`ASTType::match`) includes:

- `Any` matches any type in either direction
- generic type params are wildcard-compatible
- base type names must match unless one side is `Any`
- non-optional target does not accept optional source
- non-throwable target does not accept throwable source
- type parameter arity and each type argument must match recursively

### 3.3 Alias Resolution

Type aliases are resolved transitively during semantic checks. Generic alias parameters are substituted at use sites.

### 3.4 Task Type Constraint

`Task` must carry exactly one type argument when validated as a type expression in semantic analysis.

## 4. Declaration Semantics

### 4.1 Variables

- `decl imut` variables are compile-time readonly.
- Const variables must be initialized.
- If type is omitted, initializer type is inferred.
- If both type and initializer are missing, declaration is invalid.

### Optional/Throwable capture rule

If initializer type is optional or throwable, declaration must be secure-wrapped.

Invalid:

```starbytes
decl x = maybe()  // if maybe() -> T? or T!
```

Valid:

```starbytes
secure(decl x = maybe()) catch { ... }
```

### 4.2 Functions

- Parameter and return types are checked and alias-resolved.
- Non-native declaration-only functions are invalid.
- Native declaration-only functions must declare return type.
- Return type can be inferred from body if omitted.

### 4.3 Classes

### Inheritance semantics

- At most one superclass (class type)
- Any number of interfaces
- Inheritance list may include aliases, resolved before validation
- Circular superclass chains are rejected

### Method semantics

- Duplicate method names in same class are rejected
- Method type-check includes implicit `self` parameter
- Native declaration-only methods require explicit return type

### Constructor semantics

- Constructor overload key is arity (parameter count)
- Duplicate constructor arity is rejected
- Constructor return must be `Void` (explicit value-return is rejected)

### Interface conformance

For each implemented interface:

- required fields must exist and type-match
- required methods must exist
- return types and parameter signatures must match

Superclass members can participate in satisfying interface requirements through recursive lookup.

### 4.4 Interfaces

- Allowed members: fields and methods
- Duplicate interface method names are rejected
- Declaration-only interface methods must declare return type
- Methods with bodies are type-checked like normal methods (with implicit `self`)

### 4.5 Structs

`struct` is lowered through class machinery but syntactically restricted to fields only.

### 4.6 Enums

`enum` is lowered to a namespace scope containing constant `Int` declarations.

- default value starts at `0`
- explicit assignment sets current value
- subsequent members auto-increment from last value

### 4.7 Attributes

Validated system attributes:

- `@readonly`
  - valid only on class fields
  - no arguments
- `@deprecated`
  - valid on class/function/field declarations
  - optional single string argument
  - named key, if present, must be `message`
- `@native`
  - valid on class/function declarations
  - optional single string argument
  - named key, if present, must be `name`

## 5. Expression Semantics

### 5.1 Literals and Collections

- Array literal type is currently `Array` (element-type enforcement is limited)
- Dict literal type is `Dict`
- Dict keys must be `String`, `Int`, or `Float`

### 5.2 Member and Index Access

- Member access resolves against scope symbols, class fields/methods, and inherited members
- Index access semantics:
  - `Array[index]`: index must be `Int`
  - `Dict[key]`: key must be `String`/`Int`/`Float`

### 5.3 Unary Operators

| Operator | Static Rule | Result Type |
|---|---|---|
| `+x` | `x` numeric | numeric type of `x` |
| `-x` | `x` numeric | numeric type of `x` |
| `!x` | `x` Bool | Bool |
| `~x` | `x` Int | Int |
| `await x` | `x` is `Task<T>` | `T` |

`await` on non-task is a semantic error.

### 5.4 Binary Operators

### Arithmetic

- `+ - * / %` require numeric operands
- `+` additionally supports string concatenation if either side is `String`

### Equality

- `==` / `!=` require compatible operand types (or `Any`)
- Runtime equality supports numeric/string/bool value comparison and pointer equality fallback

### Relational

- `< <= > >=` require numeric or string pairings

### Logical

- `&&` and `||` require Bool operands
- Runtime evaluates with short-circuit semantics

### Bitwise and Shift

- `& | ^ << >>` require `Int` operands
- Runtime shift with negative RHS is treated as invalid expression result

### 5.5 Assignment Semantics

Supported assignment targets:

- variable identifier
- object member
- index target (`Array` or `Dict`)

Supported assignment operators:

- simple: `=`
- arithmetic compounds: `+= -= *= /= %=`
- bitwise compounds: `&= |= ^=`
- shift compounds: `<<= >>=`

### Assignment safety checks

- assignment type must match target type
- compound assignment computes synthetic binary result and enforces assignability back to LHS type
- assigning to const variable is rejected
- assigning to readonly field is rejected
- assigning to scope-qualified symbol is rejected

### 5.6 Runtime Type Check (`is`)

`a is T` returns `Bool`.

Accepted RHS forms:

- builtin type identifiers
- class names
- type aliases that resolve to runtime-supported types

Rejected RHS forms:

- interface types
- non-type expressions

## 6. Control-Flow Semantics

### 6.1 Conditionals

- each `if`/`elif` condition must be `Bool`
- `else` has no condition
- blocks are evaluated in order, first true branch executes

### 6.2 Loops

`while(cond){...}` and `for(cond){...}` both require `Bool` condition.

`for` currently uses the same runtime looping representation as `while`.

### 6.3 Return

- return outside function scope is invalid
- function body analysis detects unreachable statements after return

## 7. Secure/Catch Semantics

### 7.1 Static rules

- guarded declaration must contain initializer
- guarded initializer must be optional or throwable typed
- catch type, if provided, must be a valid type

### 7.2 Runtime behavior

Secure declaration bytecode holds:

- target variable id
- optional catch binding id
- optional catch type id
- guarded initializer expression
- catch block

Execution:

1. Evaluate guarded initializer
2. If value exists: bind target variable; skip catch block
3. If value missing/error: bind catch variable (if any) with runtime error string and execute catch block
4. Clear runtime error state after handling

## 8. Lazy/Task/Await Semantics

- Lazy functions compile as functions marked `isLazy`
- Invoking a lazy function schedules a microtask and returns `Task`
- Runtime microtask queue is single-threaded
- `await` drives microtask processing until task resolves/rejects
- If await cannot make progress (queue empty, task pending), runtime reports stall error

No additional worker thread is created for lazy execution.

## 9. Regex Semantics

- Regex literal compiles at runtime via PCRE2
- On success, value is `Regex` object with `pattern`/`flags`
- On compile failure, expression yields failure path compatible with secure/catch handling

## 10. Diagnostic Semantics

Diagnostics are emitted across lexer/parser/sema/runtime layers:

- semantic errors block successful compile/check
- warnings are emitted for non-fatal issues (for example, unused return values in expression statements)
- source regions are attached where available and rendered in code-view output

## 11. Current Boundaries

The following are currently outside implemented semantics:

- ternary operator semantics
- null-coalescing semantics
- full lambda literal semantics
