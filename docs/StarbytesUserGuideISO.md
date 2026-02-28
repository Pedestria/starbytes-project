# Starbytes User Guide and Language Specification (ISO-Style Draft)

Document ID: SB-UG-ISO-DRAFT  
Version: 0.11.0  
Date: February 27, 2026  
Status: Draft normative reference for Starbytes 0.11.x

## 1 Scope

This document defines the Starbytes language, its execution and error model, and the required toolchain behavior for source programs (`.starb`), interface units (`.starbint`), compiled modules (`.starbmod`), and native modules (`.ntstarbmod`).

This specification is ISO-style in structure and language ("shall", "should", "may"), but is not an ISO/IEC published standard.

## 2 Conformance

### 2.1 Conforming implementation

A conforming Starbytes implementation shall:

1. Accept and process syntax defined in this document.
2. Enforce static semantics defined in this document.
3. Execute runtime semantics defined in this document.
4. Provide `run`, `compile`, and `check` toolchain commands.
5. Support module resolution, including `.starbmodpath`.
6. Emit diagnostics for invalid programs.

### 2.2 Conforming program

A conforming program shall:

1. Use only syntax and semantics defined by this document.
2. Not depend on unspecified or implementation-defined behavior unless explicitly documented.

## 3 Normative References

Primary implementation references:

- `/Users/alextopper/Documents/GitHub/starbytes-project/StarBytesSyntax.txt`
- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/builtins.starbint`
- `/Users/alextopper/Documents/GitHub/starbytes-project/tools/driver/main.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SyntaxA.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SyntaxADecl.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SyntaxAExpr.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp`

## 4 Terms and Definitions

- `optional type (T?)`: type that may represent absence of value.
- `throwable type (T!)`: type that may represent failure/error state.
- `secure declaration`: guarded declaration with `catch` handling.
- `Task<T>`: lazy asynchronous handle resolved on `await`.
- `module`: compilable source unit (single file or directory module).
- `native module`: dynamically loaded runtime library (`.ntstarbmod`).

## 5 Language Conventions

### 5.1 Normative keywords

- "shall" indicates a requirement.
- "should" indicates a recommendation.
- "may" indicates optional behavior.

### 5.2 Character set and encoding

Source text shall be UTF-8 compatible.

### 5.3 Comments

Supported comment forms:

- line comment: `// ...`
- block comment: `/* ... */`

## 6 Lexical Structure

### 6.1 File kinds

- source file: `.starb`
- interface file: `.starbint`
- compiled module: `.starbmod`
- native module: `.ntstarbmod`

### 6.2 Keywords

`import`, `scope`, `class`, `interface`, `struct`, `enum`, `def`, `decl`, `imut`, `func`, `lazy`, `new`, `return`, `if`, `elif`, `else`, `while`, `for`, `secure`, `catch`, `await`, `is`

### 6.3 Literals

- string: `"text"`
- boolean: `true`, `false`
- integer: `42`
- floating: `3.14`
- regex: `/(pattern)/flags`

Regex literal parsing shall preserve escaped delimiters and flags.

### 6.4 Operators

- arithmetic: `+ - * / %`
- assignment: `= += -= *= /= %= &= |= ^= <<= >>=`
- equality: `== !=`
- relational: `< <= > >=`
- logical: `&& || !`
- bitwise: `& | ^ ~ << >>`
- ternary: `?:`
- type check: `is`
- unary async: `await`

## 7 Type System

### 7.1 Builtin types

Builtin scalar/runtime types:

- `Void`, `Bool`, `Int`, `Long`, `Float`, `Double`, `String`, `Regex`, `Any`

Builtin generic/container types:

- `Array<T>`
- `Dict`
- `Map<K,V>`
- `Task<T>`

Function types are first-class via function type syntax:

- `(a:Int,b:Int) Int`
- `(Int,Int) Int`

### 7.2 Type references

Type references shall support:

1. named types (`String`, `MyClass`, `MyType<T>`)
2. function types (`(params...) ReturnType`)
3. array suffix forms (`T[]`, `T[][]`, ...)
4. qualifiers (`?`, `!`, or both)

Examples:

- `Int?`
- `Regex!`
- `Task<String>`
- `Map<String,Int[]>`
- `(value:Int) Bool`

### 7.3 Type aliases

`def` declares a type alias.

Example:

```starbytes
def UserId = Int
def Handler = (msg:String) Void
```

Generic aliases are supported:

```starbytes
def Box<T> = Array<T>
```

### 7.4 Type compatibility

The implementation shall enforce:

1. assignment compatibility by declared type.
2. optional values (`T?`) shall not flow into non-optional targets without secure handling.
3. throwable values (`T!`) shall not flow into non-throwable targets without secure handling.
4. `Any` acts as top-type compatibility bridge.
5. map key types shall be `String` or numeric (`Int`, `Long`, `Float`, `Double`) or compatible generic constraints.

### 7.5 Numeric literal inference

Default numeric inference mode:

- integer literals -> `Int`
- floating literals -> `Float`

With `--infer-64bit-numbers`:

- integer literals -> `Long`
- floating literals -> `Double`

## 8 Declarations and Scopes

### 8.1 Import declarations

```starbytes
import IO
import Net
```

Imports are module-name based (identifier form).

### 8.2 Namespace scopes

`scope` defines a namespace.

```starbytes
scope Core {
    func answer() Int { return 42 }
}
```

### 8.3 Variable declarations

Forms:

- `decl name:Type`
- `decl name = expr`
- `decl name:Type = expr`
- `decl imut name = expr`

Implied array declaration syntax is supported:

- `decl values[] = [1,2,3]`

### 8.4 Function declarations

Forms:

- `func name(params) ReturnType { ... }`
- `lazy name(params) ReturnType { ... }`
- `lazy func name(params) ReturnType { ... }`

Rules:

1. Parameters are `name:Type`.
2. Return type may be omitted and inferred where permitted.
3. Lazy functions evaluate as `Task<ReturnType>` at call sites.

### 8.5 Inline function values

Inline function expression forms are supported in expression contexts:

- `(Int a, Int b) Int { ... }`
- `(a:Int,b:Int) Int { ... }`
- `func (a:Int,b:Int) Int { ... }`

Typed inline function variable form:

```starbytes
def Combiner = (a:Int,b:Int) Int
decl func add:Combiner = (Int a, Int b) Int { return a + b }
```

### 8.6 Class declarations

```starbytes
class Worker : Base, Runnable {
    decl value:Int
    new(v:Int){ self.value = v }
    func get() Int { return self.value }
}
```

Inheritance list semantics:

1. At most one class superclass.
2. Remaining entries shall be interfaces.
3. Interface conformance shall be type-checked.

### 8.7 Interface declarations

Interfaces may contain fields and methods (declaration-only or with body).

```starbytes
interface Named {
    decl name:String
    func label() String
}
```

### 8.8 Struct declarations

`struct` declarations are field-only declarations.

```starbytes
struct Point {
    decl x:Int
    decl y:Int
}
```

### 8.9 Enum declarations

```starbytes
enum Status {
    OK
    WARN = 3
    FAIL
}
```

Enum members lower to constant `Int` values with auto-increment behavior.

### 8.10 Constructors

Constructors are class-only and use `new(...) { ... }`.

### 8.11 Attributes

System attributes:

- `@readonly`
- `@deprecated(...)`
- `@native(...)`

Attribute validation shall enforce declaration compatibility and argument shape.

## 9 Expressions and Operators

### 9.1 Primary expressions

- identifiers
- literals
- parenthesized expressions
- array literals (`[...]`)
- dictionary literals (`{ key: value, ... }`)
- constructor calls (`new Type(args)`)
- inline function expressions

### 9.2 Postfix expressions

- member access: `a.b`
- invocation: `f(x,y)`
- indexing: `x[i]`

### 9.3 Operator precedence (highest to lowest)

1. postfix (`.`, call, index)
2. unary (`+`, `-`, `!`, `~`, `await`)
3. multiplicative (`*`, `/`, `%`)
4. additive (`+`, `-`)
5. shift (`<<`, `>>`)
6. relational (`<`, `<=`, `>`, `>=`, `is`)
7. equality (`==`, `!=`)
8. bitwise and (`&`)
9. bitwise xor (`^`)
10. bitwise or (`|`)
11. logical and (`&&`)
12. logical or (`||`)
13. ternary (`?:`)
14. assignment (`=`, compound assignments)

### 9.4 Assignment

Valid assignment targets:

- variable identifiers
- object members
- indexable container entries (`Array`, `Dict`, `Map`)

Compound assignments are supported.

### 9.5 Runtime type check (`is`)

`value is Type` returns `Bool`.

Allowed RHS:

- builtin type names
- class types
- aliases resolving to runtime-supported types

Rejected RHS:

- interface types
- non-type expressions

### 9.6 Cast invocations

Type-cast invocation syntax uses type-call form:

- `Int(x)`, `Long(x)`, `Float(x)`, `Double(x)`, `Bool(x)`, `String(x)`, `Any(x)`
- class-cast via class-type call form

Class downcasts may become throwable and shall be handled accordingly.

## 10 Statements and Control Flow

### 10.1 Conditionals

`if` / `elif` / `else` conditions shall be `Bool`.

### 10.2 Loops

Supported loop forms:

- `while(condition) { ... }`
- `for(condition) { ... }`

Current `for` is condition-loop form, not C-style three-clause `for(init;cond;step)`.

### 10.3 Return

`return` may appear only in function/lazy function bodies.

## 11 Error and Recovery Model

### 11.1 Secure declarations

Primary form:

```starbytes
secure(decl value = maybe()) catch (err:String) {
    print(err)
}
```

Declaration sugar form:

```starbytes
decl value = maybe() catch (err:String) {
    print(err)
}
```

Rules:

1. Guarded declaration shall include an initializer.
2. Guarded initializer should evaluate to optional or throwable value.
3. Catch block executes on absent/failing result.
4. Catch binding type, when present, shall be a valid type.

### 11.2 Diagnostics

Implementations shall produce diagnostics for parser, semantic, and runtime violations.  
Diagnostic code standardization is in active optimization roadmap phases; message and span output remain normative for user-facing behavior.

## 12 Lazy/Task/Await Semantics

1. Invoking a `lazy` function schedules a task and returns `Task<T>`.
2. `await` requires `Task<T>` and yields `T`.
3. Task execution model is single-threaded (microtask queue); no additional worker thread is required.
4. Await on an unresolved task with no further progress opportunity shall report runtime failure.

## 13 Modules, Resolution, and Artifacts

### 13.1 Module forms

Starbytes accepts:

- single-file roots
- directory module roots

### 13.2 `.starbmodpath`

When present, `.starbmodpath` provides additional module search directories (one path per line).  
Blank lines and comment lines (`#`, `//`) are ignored.  
Relative entries are resolved from the `.starbmodpath` containing directory.

### 13.3 Import resolution

Resolution uses local and configured roots, including workspace defaults and `.starbmodpath` roots, with deterministic first-match behavior.

### 13.4 Outputs

Compile outputs include:

- `.starbmod` compiled module
- `.starbsymtb` symbol table artifact
- `.starbint` interface output for library-style module compilation
- cache entries under local `.starbytes/.cache`

`--clean` removes generated output artifacts and compile cache on success.

## 14 Builtin API Surface (Normative)

Builtin API is defined by `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/builtins.starbint`.

### 14.1 Global builtin function

```starbytes
func print(object:Any) Void
```

### 14.2 Builtin interfaces

`String`:

- `length:Int`
- `isEmpty() Bool`
- `at(index:Int) String?`
- `slice(start:Int,end:Int) String`
- `contains(value:String) Bool`
- `startsWith(prefix:String) Bool`
- `endsWith(suffix:String) Bool`
- `indexOf(value:String) Int`
- `lastIndexOf(value:String) Int`
- `lower() String`
- `upper() String`
- `trim() String`
- `replace(oldValue:String,newValue:String) String`
- `split(separator:String) Array<String>`
- `repeat(count:Int) String`

`Array<T>`:

- `length:Int`
- `isEmpty() Bool`
- `push(value:T) Int`
- `pop() T?`
- `at(index:Int) T?`
- `set(index:Int,value:T) Bool`
- `insert(index:Int,value:T) Bool`
- `removeAt(index:Int) T?`
- `clear() Bool`
- `contains(value:T) Bool`
- `indexOf(value:T) Int`
- `slice(start:Int,end:Int) Array<T>`
- `join(separator:String) String`
- `copy() Array<T>`
- `reverse() Array<T>`

`Dict`:

- `length:Int`
- `isEmpty() Bool`
- `has(key:DictKey) Bool`
- `get(key:DictKey) Any?`
- `set(key:DictKey,value:Any) Bool`
- `remove(key:DictKey) Any?`
- `keys() Array`
- `values() Array`
- `clear() Bool`
- `copy() Dict`

`Map<K,V>`:

- `length:Int`
- `isEmpty() Bool`
- `has(key:K) Bool`
- `get(key:K) V?`
- `set(key:K,value:V) Bool`
- `remove(key:K) V?`
- `keys() Array<K>`
- `values() Array<V>`
- `clear() Bool`
- `copy() Map<K,V>`

`Task<T>`:

- marker asynchronous result type consumed by `await`

## 15 Standard Library Modules

The following module interfaces are part of the 0.11.x stdlib baseline (native-backed where applicable):

- `Archive`
- `CmdLine`
- `Compression`
- `Config`
- `Crypto`
- `Env`
- `FS`
- `HTTP`
- `IO`
- `JSON`
- `Log`
- `Net`
- `Process`
- `Random`
- `Text`
- `Threading`
- `Time`
- `Unicode`

Each module's authoritative API contract is its `.starbint` file under:

- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/<ModuleName>/<ModuleName>.starbint`

## 16 Toolchain Contract

### 16.1 Commands

- `run`: compile and execute (default)
- `compile`: compile only (optional execute with `--run`)
- `check`: parse + semantic checks only

### 16.2 Key options

- `--native`, `--native-dir`, `--no-native-auto`
- `--modulename`, `--output`, `--out-dir`, `--print-module-path`
- `--clean`
- `--profile-compile`, `--profile-compile-out`
- `--infer-64bit-numbers`
- `--no-diagnostics`
- `-- <args...>` to forward script arguments

## 17 Implementation-Defined and Unsupported Areas

The following are explicitly outside fully finalized language surface:

1. Lambda literal surface from early design drafts (not standardized in this document).
2. Null-coalescing operator family.
3. C-style `for(init;cond;step)` form.

The following remain implementation-defined:

1. Certain runtime representation details for objects and closures.
2. Exact diagnostic wording for some equivalent error classes (diagnostic code unification in progress).

## Annex A (Normative): EBNF Grammar

```ebnf
program               = { declaration | expression } ;

declaration           = importDecl
                      | scopeDecl
                      | enumDecl
                      | typeAliasDecl
                      | varDecl
                      | funcDecl
                      | classDecl
                      | interfaceDecl
                      | structDecl
                      | constructorDecl
                      | conditionalDecl
                      | loopDecl
                      | secureDecl
                      | returnDecl ;

importDecl            = "import" Identifier ;
scopeDecl             = "scope" Identifier block ;
enumDecl              = "enum" Identifier "{" enumMember { ["," ] enumMember } ["," ] "}" ;
enumMember            = Identifier [ "=" ["-"] IntegerLiteral ] ;

typeAliasDecl         = "def" Identifier [genericParams] "=" typeRef ;
genericParams         = "<" Identifier { "," Identifier } ">" ;

typeRef               = functionType | namedType ;
functionType          = "(" [functionTypeParamList] ")" typeRef ;
functionTypeParamList = functionTypeParam { "," functionTypeParam } ;
functionTypeParam     = Identifier ":" typeRef
                      | typeRef [Identifier] ;

namedType             = Identifier [genericArgs] { "[]" } { "?" | "!" } ;
genericArgs           = "<" typeRef { "," typeRef } ">" ;

varDecl               = "decl" ["imut"] Identifier ["[]"] [ ":" typeRef ] [ "=" expression ] ;
inlineVarDecl         = "decl" "func" Identifier ":" typeRef "=" inlineFuncExpr ;

funcDecl              = ("func" | "lazy" ["func"]) Identifier "(" [paramList] ")" [typeRef] (block | declarationOnly) ;
constructorDecl       = "new" "(" [paramList] ")" block ;

paramList             = param { "," param } ;
param                 = Identifier ":" typeRef ;

classDecl             = "class" Identifier [genericParams] [ ":" typeRef { "," typeRef } ] "{"
                        { varDecl | funcDecl | constructorDecl } "}" ;

interfaceDecl         = "interface" Identifier [genericParams] "{"
                        { varDecl | funcDecl } "}" ;

structDecl            = "struct" Identifier [genericParams] "{"
                        { varDecl } "}" ;

conditionalDecl       = "if" "(" expression ")" block
                        { "elif" "(" expression ")" block }
                        [ "else" block ] ;

loopDecl              = ("while" | "for") "(" expression ")" block ;

secureDecl            = "secure" "(" varDeclWithInitializer ")" "catch" [catchBinding] block
                      | varDeclWithInitializer "catch" [catchBinding] block ;
varDeclWithInitializer= "decl" ["imut"] Identifier [ ":" typeRef ] "=" expression ;
catchBinding          = "(" Identifier ":" typeRef ")" ;

returnDecl            = "return" [expression] ;
block                 = "{" { declaration | expression } "}" ;
declarationOnly       = ;

expression            = assignmentExpr ;
assignmentExpr        = ternaryExpr [ assignOp assignmentExpr ] ;
assignOp              = "=" | "+=" | "-=" | "*=" | "/=" | "%="
                      | "&=" | "|=" | "^=" | "<<=" | ">>=" ;

ternaryExpr           = logicOrExpr [ "?" assignmentExpr ":" assignmentExpr ] ;
logicOrExpr           = logicAndExpr { "||" logicAndExpr } ;
logicAndExpr          = bitwiseOrExpr { "&&" bitwiseOrExpr } ;
bitwiseOrExpr         = bitwiseXorExpr { "|" bitwiseXorExpr } ;
bitwiseXorExpr        = bitwiseAndExpr { "^" bitwiseAndExpr } ;
bitwiseAndExpr        = equalityExpr { "&" equalityExpr } ;
equalityExpr          = relationalExpr { ("==" | "!=") relationalExpr } ;
relationalExpr        = shiftExpr { ("<" | "<=" | ">" | ">=" | "is") shiftExpr } ;
shiftExpr             = additiveExpr { ("<<" | ">>") additiveExpr } ;
additiveExpr          = multiplicativeExpr { ("+" | "-") multiplicativeExpr } ;
multiplicativeExpr    = unaryExpr { ("*" | "/" | "%") unaryExpr } ;
unaryExpr             = ( "+" | "-" | "!" | "~" | "await" ) unaryExpr
                      | postfixExpr ;

postfixExpr           = primaryExpr { "." Identifier | "(" [argList] ")" | "[" expression "]" } ;
argList               = expression { "," expression } ;

primaryExpr           = Identifier
                      | literal
                      | inlineFuncExpr
                      | "(" expression ")"
                      | "[" [argList] "]"
                      | "{" [dictEntry { "," dictEntry }] "}"
                      | "new" scopedIdentifier [genericArgs] "(" [argList] ")" ;

literal               = StringLiteral
                      | IntegerLiteral
                      | FloatingLiteral
                      | BooleanLiteral
                      | RegexLiteral ;

inlineFuncExpr        = ["func"] "(" [inlineParamList] ")" typeRef block ;
inlineParamList       = inlineParam { "," inlineParam } ;
inlineParam           = Identifier ":" typeRef
                      | typeRef Identifier ;

dictEntry             = expression ":" expression ;
scopedIdentifier      = Identifier { "." Identifier } ;
```

## Annex B (Informative): Migration Notes for Existing Docs

This document supersedes older split references for syntax and semantics where conflicts exist.

When old documents differ from this guide, this guide is authoritative for 0.11.x behavior.
