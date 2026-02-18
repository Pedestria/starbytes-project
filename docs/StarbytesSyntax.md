# Starbytes Syntax Reference

Last updated: February 18, 2026

This document describes the concrete Starbytes syntax that is implemented in the current codebase.

## 1. Source Files

- Source file extension: `.starb`
- Interface file extension: `.starbint`
- Compiled module extension: `.starbmod`
- Native module extension: `.ntstarbmod`

## 2. Lexical Rules

### 2.1 Keywords

`import`, `class`, `struct`, `interface`, `enum`, `lazy`, `await`, `is`, `decl`, `func`, `return`, `imut`, `if`, `else`, `elif`, `for`, `while`, `secure`, `catch`, `new`, `scope`, `def`

### 2.2 Literals

- String: `"hello"`
- Boolean: `true`, `false`
- Integer: `42`
- Float: `3.14`
- Regex: `/(pattern)/flags`

Regex flags recognized by runtime compilation include `i`, `m`, `s`, `u`.

### 2.3 Comments

- Line comment: `// ...`
- Block comment: `/* ... */`

### 2.4 Punctuation

`(` `)` `[` `]` `{` `}` `,` `:` `.` `@`

### 2.5 Operators

- Arithmetic: `+ - * / %`
- Assignment: `= += -= *= /= %= &= |= ^= <<= >>=`
- Equality: `== !=`
- Relational: `< <= > >=`
- Logical: `&& || !`
- Bitwise: `& | ^ ~ << >>`
- Type check: `is`

## 3. Types

### 3.1 Built-in Types

`Void`, `String`, `Bool`, `Array`, `Dict`, `Int`, `Float`, `Regex`, `Any`, `Task`

### 3.2 Generic Type Arguments

Generic syntax is supported on type references:

- `Task<String>`
- `Box<Int>`
- `Map<String,Int>` (for user-defined generic aliases/classes/interfaces)

### 3.3 Type Qualifiers

- Optional: `T?`
- Throwable: `T!`
- Both: `T?!` (order is parser-accepted as repeated suffix markers)

## 4. Attributes

Attributes use `@` before declarations.

Examples:

- `@readonly`
- `@deprecated("message")`
- `@native(name="symbol")`

Attribute arguments support:

- positional form: `@deprecated("old")`
- named form: `@native(name="person_getName")`

## 5. Declarations

### 5.1 Import

```starbytes
import IO
import Time
```

Import names are identifiers only.

### 5.2 Scope (Namespace)

```starbytes
scope Core {
    decl greeting:String = "hello"

    scope Math {
        func answer() Int {
            return 42
        }
    }
}
```

### 5.3 Enum

```starbytes
enum Priority {
    LOW
    MEDIUM = 3
    HIGH
}
```

Enum members become constant `Int` values in a generated namespace scope.

### 5.4 Type Alias

```starbytes
def UserId = Int
def Result<T> = Task<T>
```

### 5.5 Variables and Constants

```starbytes
decl x:Int = 1
decl y = "hello"
decl imut z = 5
```

### 5.6 Functions

```starbytes
func add(a:Int,b:Int) Int {
    return a + b
}
```

Lazy functions:

```starbytes
lazy delayedAdd(a:Int,b:Int) Int {
    return a + b
}
```

`lazy func name(...)` form is also accepted.

### 5.7 Constructors

Constructors are class-only and use `new`:

```starbytes
class User {
    decl name:String

    new(name:String){
        self.name = name
    }
}
```

### 5.8 Class

```starbytes
class Child : Parent, InterfaceA, InterfaceB {
    decl value:Int

    new(v:Int){
        self.value = v
    }

    func getValue() Int {
        return self.value
    }
}
```

### 5.9 Interface

```starbytes
interface Named<T> {
    decl value:T

    func get() T {
        return self.value
    }
}
```

Interface methods may be declaration-only or have bodies.

### 5.10 Struct

```starbytes
struct Point {
    decl x:Int
    decl y:Int
}
```

Struct syntax maps to a class-like declaration constrained to field declarations.

### 5.11 Conditional

```starbytes
if(flag){
    print("A")
}
elif(other){
    print("B")
}
else {
    print("C")
}
```

### 5.12 Loops

```starbytes
while(cond){
    work()
}

for(cond){
    work()
}
```

Current `for` is condition-style (`for(<bool-expr>)`) rather than a C-style three-clause loop.

### 5.13 Secure/Catch

Primary form:

```starbytes
secure(decl result = maybeValue()) catch (error:String) {
    print(error)
}
```

Sugar form (wrapped variable declaration):

```starbytes
decl result = maybeValue() catch (error:String) {
    print(error)
}
```

### 5.14 Return

```starbytes
return
return expr
```

## 6. Expressions

### 6.1 Primary Expressions

- literals
- identifier
- parenthesized expression: `(expr)`
- array literal: `[a,b,c]`
- dictionary literal: `{"k":1, "v":2}`
- constructor invocation: `new ClassName(args)`
- scoped constructor: `new Scope.Class(args)`

### 6.2 Postfix Expressions

- Member access: `obj.field`
- Call: `fn(arg1,arg2)`
- Index: `arr[i]`, `dict[key]`
- Chaining: `obj.method()[i].field`

### 6.3 Operator Precedence and Associativity

Highest to lowest:

1. Postfix: member/call/index (left)
2. Unary: `+ - ! ~ await` (right)
3. Multiplicative: `* / %` (left)
4. Additive: `+ -` (left)
5. Shift: `<< >>` (left)
6. Relational: `< <= > >= is` (left)
7. Equality: `== !=` (left)
8. Bitwise AND: `&` (left)
9. Bitwise XOR: `^` (left)
10. Bitwise OR: `|` (left)
11. Logical AND: `&&` (left)
12. Logical OR: `||` (left)
13. Assignment: `= += -= *= /= %= &= |= ^= <<= >>=` (right)

### 6.4 Assignment Targets

Valid assignment LHS forms:

- variable identifier: `x = 1`
- object member: `obj.value = 1`
- indexed container: `arr[0] = 1`, `dict["k"] = 1`

### 6.5 Runtime Type Check

```starbytes
if(value is String){
    print("string")
}
```

`is` accepts builtin type names and class/type-alias targets. Interface targets are rejected.

## 7. Grammar (EBNF-Oriented)

```ebnf
program            = { declaration | expression } ;

declaration        = importDecl
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

importDecl         = "import" Identifier ;
scopeDecl          = "scope" Identifier block ;
enumDecl           = "enum" Identifier "{" enumMember { "," enumMember } "}" ;
enumMember         = Identifier [ "=" ["-"] IntegerLiteral ] ;

typeAliasDecl      = "def" Identifier [genericParams] "=" typeRef ;

genericParams      = "<" Identifier { "," Identifier } ">" ;

typeRef            = Identifier [ "<" typeRef { "," typeRef } ">" ] [ "?" ] [ "!" ] ;

varDecl            = "decl" ["imut"] Identifier [ ":" typeRef ] [ "=" expression ] ;

funcDecl           = ["lazy"] ["func"] Identifier "(" [paramList] ")" [typeRef] (block | declarationOnly) ;
constructorDecl    = "new" "(" [paramList] ")" block ;
paramList          = param { "," param } ;
param              = Identifier ":" typeRef ;

classDecl          = "class" Identifier [genericParams] [":" typeRef { "," typeRef }] "{" { classMember } "}" ;
classMember        = varDecl | funcDecl | constructorDecl ;

interfaceDecl      = "interface" Identifier [genericParams] "{" { interfaceMember } "}" ;
interfaceMember    = varDecl | funcDecl ;

structDecl         = "struct" Identifier [genericParams] "{" { varDecl } "}" ;

conditionalDecl    = "if" "(" expression ")" block
                     { "elif" "(" expression ")" block }
                     [ "else" block ] ;

loopDecl           = ("while" | "for") "(" expression ")" block ;

secureDecl         = "secure" "(" varDeclWithInitializer ")" "catch" [ "(" Identifier ":" typeRef ")" ] block ;
varDeclWithInitializer = "decl" ["imut"] Identifier [ ":" typeRef ] "=" expression ;

returnDecl         = "return" [expression] ;

block              = "{" { declaration | expression } "}" ;

declarationOnly    = ;

expression         = assignmentExpr ;
assignmentExpr     = logicOrExpr [ assignOp assignmentExpr ] ;
assignOp           = "=" | "+=" | "-=" | "*=" | "/=" | "%="
                   | "&=" | "|=" | "^=" | "<<=" | ">>=" ;

logicOrExpr        = logicAndExpr { "||" logicAndExpr } ;
logicAndExpr       = bitwiseOrExpr { "&&" bitwiseOrExpr } ;
bitwiseOrExpr      = bitwiseXorExpr { "|" bitwiseXorExpr } ;
bitwiseXorExpr     = bitwiseAndExpr { "^" bitwiseAndExpr } ;
bitwiseAndExpr     = equalityExpr { "&" equalityExpr } ;

equalityExpr       = relationalExpr { ("==" | "!=") relationalExpr } ;
relationalExpr     = shiftExpr { ("<" | "<=" | ">" | ">=" | "is") shiftExpr } ;
shiftExpr          = additiveExpr { ("<<" | ">>") additiveExpr } ;
additiveExpr       = multiplicativeExpr { ("+" | "-") multiplicativeExpr } ;
multiplicativeExpr = unaryExpr { ("*" | "/" | "%") unaryExpr } ;
unaryExpr          = ["+" | "-" | "!" | "~" | "await"] unaryExpr | postfixExpr ;

postfixExpr        = primaryExpr { "." Identifier | "(" [argList] ")" | "[" expression "]" } ;
argList            = expression { "," expression } ;

primaryExpr        = Identifier
                   | literal
                   | "(" expression ")"
                   | "[" [argList] "]"
                   | "{" [dictEntry { "," dictEntry }] "}"
                   | "new" scopedIdentifier ["<" typeRef { "," typeRef } ">"] "(" [argList] ")" ;

dictEntry          = expression ":" expression ;
scopedIdentifier   = Identifier { "." Identifier } ;
```

## 8. Unsupported or Not-Yet-Specified Syntax

The following are not complete language features in the current implementation:

- ternary `?:`
- null-coalescing operators
- lambda literal syntax from design drafts
- C-style `for(init;cond;step)` loop form
