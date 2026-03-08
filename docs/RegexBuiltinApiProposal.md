# Regex Builtin API Proposal

## Goal

Keep regex as a builtin language surface, not a stdlib module utility.

That means:
- regex literals continue to produce `Regex`
- regex operations live on `Regex` values
- `Text` remains focused on text transforms, locale, and segmentation

## Current Implemented Slice

Implemented now:

```starbytes
interface Regex {
    func match(text:String) Bool!
    func findAll(text:String) Array<String>!
    func replace(text:String,replacement:String) String!
}
```

Notes:
- all regex operations remain throwable because literal compilation and method execution can fail
- `findAll` returns matched text only, not capture metadata
- `replace` performs global replacement of every match

## Why This Direction

- It matches the language model better: regex literals already produce a first-class `Regex`
- It removes the awkward `Text.regex*(pattern,text,...)` shape
- It makes future expansion composable without growing `Text` into a mixed text-plus-regex namespace

## Proposed Complete API

### Phase 1: Metadata + Core Query

```starbytes
interface Regex {
    decl pattern:String
    decl flags:String

    func match(text:String) Bool!
    func find(text:String) RegexMatch?!
    func findAll(text:String) Array<String>!
    func findAllMatches(text:String) Array<RegexMatch>!
    func count(text:String) Int!
    func split(text:String) Array<String>!
    func replace(text:String,replacement:String) String!
    func replaceFirst(text:String,replacement:String) String!
}
```

Rationale:
- `pattern` and `flags` expose already-present runtime metadata
- `find` and `findAllMatches` are needed for anything beyond whole-match text extraction
- `split` is a standard regex affordance and belongs on `Regex`, not `String`
- `replaceFirst` avoids forcing callers into over-broad global replacement

### Phase 2: Match/Capture Model

```starbytes
interface RegexMatch {
    decl value:String
    decl start:Int
    decl end:Int
    decl groups:Array<String?>

    func group(index:Int) String?
    func groupRange(index:Int) Int[]?
    func namedGroup(name:String) String?
    func namedGroupRange(name:String) Int[]?
}
```

Rationale:
- capture access is the main gap between the current slice and a usable regex API
- index ranges are required for source transforms and editor tooling
- named groups make complex expressions maintainable

### Phase 3: Replacement Features

```starbytes
interface Regex {
    func replaceWith(text:String,replacer:(RegexMatch) String) String!
}
```

Rationale:
- callback replacement is the practical end-state for a complete regex API
- it should be deferred until `RegexMatch` is stable because the callback surface depends on it

### Phase 4: Validation/Construction Helpers

Two viable options:

1. Keep literal-first construction only.
2. Add a construction helper after builtin static/member scope rules are clarified.

If constructor-like builtin helpers are added later, prefer:

```starbytes
func regex(pattern:String,flags:String) Regex!
```

over a fake module helper, because the value model is still builtin.

## Runtime Architecture Recommendation

### 1. Keep Pattern Metadata on `Regex`

Current object layout already stores:
- `pattern`
- `flags`

That should remain the public source of truth.

### 2. Add Lazy Compiled-Pattern Caching

Current behavior recompiles PCRE2 patterns on each method call. That is correct but wasteful.

Recommended next runtime step:
- store a lazily-initialized native compiled handle on the `Regex` object
- invalidate only if object construction semantics ever become mutable
- free the handle during object release

This should be the first performance step after the API surface is stable.

### 3. Keep Methods Throwable

Regex methods should continue to use native-error plumbing for:
- invalid compiled state
- invalid replacement forms once capture substitutions are added
- backend failures

## Compiler Work Plan

### Wave A

- Add `Regex` builtin method/property signatures in semantic analysis
- Move current `Text.regex*` behavior to builtin runtime dispatch
- Remove the old `Text.regex*` exports

Status: implemented.

### Wave B

- Add `Regex.pattern` and `Regex.flags` builtin properties
- Add `find`, `split`, `replaceFirst`, `count`
- Add tests for no-match, zero-width-match, and multiline flag behavior

### Wave C

- Add `RegexMatch` runtime object and interface extraction
- Implement capture groups and named groups
- Return structured matches from `find` and `findAllMatches`

### Wave D

- Add callback replacement
- Add compiled-pattern caching on regex objects
- Add benchmark coverage for repeated regex reuse

## Testing Requirements

For a complete regex API, dedicated regressions should cover:
- zero-width matches
- repeated global matches with advancing offsets
- multiline and dotall flags
- unicode flag handling
- replacement on empty and no-match inputs
- capture indexing
- named groups
- invalid pattern failure paths
- repeated calls on the same regex value to validate caching correctness

## Recommendation

Build the API in this order:

1. move/lock the builtin method surface
2. expose metadata
3. add structured match results
4. add callback replacement
5. add compiled-regex caching

Do not add a separate regex stdlib module. The language already has the right abstraction: `Regex` is a builtin value type.
