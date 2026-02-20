# Starbytes Builtins

This reference documents intrinsic (non-stdlib-module) language types and members currently implemented by the compiler/runtime.

Source of truth used for this document:
- `src/compiler/ExprSema.cpp` (type checking and builtin member signatures)
- `src/runtime/RTEngine.cpp` (runtime builtin method behavior)

## Global Intrinsic Functions

```starbytes
func print(object:Any) Void
```

## Intrinsic Types

### `Void`
- No members.

### `Bool`
- No builtin members (operator/type-check support only).

### `Int`
- No builtin members (operator/type-check support only).

### `Float`
- No builtin members (operator/type-check support only).

### `Regex`
- No direct builtin members.
- Produced by regex literals (for example `/abc/g`).

### `Any`
- Dynamic top type for values.
- No direct builtin members.

### `Task<T>`
- Produced by `lazy` functions.
- Consumed by `await`.
- No direct builtin members yet.

## `String` Members

```starbytes
decl length:Int
func isEmpty() Bool
func at(index:Int) String?
func slice(start:Int,end:Int) String
func contains(value:String) Bool
func startsWith(prefix:String) Bool
func endsWith(suffix:String) Bool
func indexOf(value:String) Int
func lastIndexOf(value:String) Int
func lower() String
func upper() String
func trim() String
func replace(oldValue:String,newValue:String) String
func split(separator:String) Array<String>
func repeat(count:Int) String
```

## `Array<T>` Members

```starbytes
decl length:Int
func isEmpty() Bool
func push(value:T) Int
func pop() T?
func at(index:Int) T?
func set(index:Int,value:T) Bool
func insert(index:Int,value:T) Bool
func removeAt(index:Int) T?
func clear() Bool
func contains(value:T) Bool
func indexOf(value:T) Int
func slice(start:Int,end:Int) Array<T>
func join(separator:String) String
func copy() Array<T>
func reverse() Array<T>
```

## `Dict` Members

Dict keys are restricted to:

```starbytes
def DictKey = Int | Float | String
```

Supported members:

```starbytes
decl length:Int
func isEmpty() Bool
func has(key:DictKey) Bool
func get(key:DictKey) Any?
func set(key:DictKey,value:Any) Bool
func remove(key:DictKey) Any?
func keys() Array
func values() Array
func clear() Bool
func copy() Dict
```

## Related Semantics

- `is` runtime type checks support builtin names (for example `String`, `Int`, `Task`, `Regex`, `Any`).
- `lazy func f(...) T` has effective invocation type `Task<T>`.
- `await task` requires `Task<T>` and yields `T`.
- Optional return members (`?`) should be handled via `secure(...) catch { ... }` flow where required by your code path.
