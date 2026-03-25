# Unicode Stdlib Completion Plan

## Goal

Make `Unicode` a complete, production-grade Unicode library for Starbytes that:

- complements the builtin `String` type rather than duplicating it
- uses ICU where locale- and boundary-sensitive behavior matters
- keeps hot scalar operations in the core runtime when ICU is unnecessary
- matches Starbytes semantics:
  - scoped module API
  - value-oriented data flow
  - `?` for absence
  - `!` for fallible operations
  - immutable-by-default usage patterns
  - strong typed return values instead of stringly-typed status codes

This should feel like:

- Swift in how it distinguishes scalar/grapheme/locale-sensitive text semantics
- Python in how it exposes practical normalization, category, lookup, and text-analysis utilities

## Current State

Current `Unicode` surface in `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/Unicode/Unicode.starbint` already provides:

- normalization:
  - `normalize`
  - `isNormalized`
- locale and casing:
  - `localeCurrent`
  - `localeRoot`
  - `localeFrom`
  - `toUpper`
  - `toLower`
  - `toTitle`
  - `caseFold`
- segmentation:
  - `graphemes`
  - `words`
  - `sentences`
  - `lines`
  - `breakIteratorCreate`
  - `BreakIterator.boundaries`
- collation:
  - `collatorCreate`
  - `Collator.compare`
  - `Collator.sortKey`
- scalar conversion and metadata:
  - `codepoints`
  - `fromCodepoints`
  - `scalarInfo`
  - `UnicodeScalarInfo.codepoint`
  - `UnicodeScalarInfo.name`
  - `UnicodeScalarInfo.category`
  - `UnicodeScalarInfo.script`
- folded matching:
  - `containsFolded`
  - `startsWithFolded`
  - `endsWithFolded`
- display:
  - `displayWidth`

Recent runtime work also moved builtin `String` indexing and length to Unicode scalar semantics.

## Current Gaps

The current module is useful but not complete. The main gaps are:

1. Scalar metadata is too thin.
2. There is no first-class grapheme abstraction.
3. There is no Unicode-aware index/range conversion layer between:
   - scalar indices
   - grapheme indices
   - byte offsets
4. Break iteration is low-level and exposes only boundaries.
5. There is no normalization/case/collation options model.
6. There is no Unicode property/query API comparable to Python `unicodedata`.
7. There is no locale-aware search/compare API beyond simple folded contains/prefix/suffix.
8. There is no transcoding or encoding validation surface.
9. There is no identifier/security-oriented Unicode support.
10. There is no regex/Unicode integration layer even though builtin `Regex` exists.

## Design Principles

### 1. Keep builtin `String` simple

Do not move the entire Unicode API onto `String`.

Builtin `String` should keep:

- scalar length
- scalar indexing
- basic scalar slicing
- ASCII-ish convenience operations already in runtime

`Unicode` should own:

- locale-sensitive behavior
- segmentation
- normalization
- Unicode properties
- collation
- script/category/bidi/width analysis
- advanced conversions

### 2. Separate scalar and grapheme semantics explicitly

Do not blur these:

- Unicode scalar value
- grapheme cluster
- word/sentence/line break
- byte offset

That separation is the core reason to keep a dedicated Unicode module.

### 3. Prefer typed result objects over parallel arrays

Avoid APIs that return raw `Array<Int>` plus “interpret this yourself”.

Prefer:

- `UnicodeScalarInfo`
- `Grapheme`
- `BreakSegment`
- `CollationKey`
- `TextRange`

### 4. Keep failure model consistent

Use:

- `?` for lookups that may legitimately miss:
  - by-name character lookup
  - property lookup by unknown alias
  - optional boundary conversion
- `!` for operational failures:
  - invalid locale
  - invalid normalization form
  - malformed encoding input
  - unsupported transliterator id

### 5. Use ICU where it is the correct engine

ICU should back:

- normalization
- locale casing
- collation
- grapheme/word/sentence/line segmentation
- script/category/bidi/name/property data
- transliteration
- charset transcoding

Core runtime should continue to own:

- UTF-8 scalar counting
- scalar indexing for builtin `String`

## Recommended API Shape

## Phase Layout Summary

1. Phase 1: Scalar and property completion
2. Phase 2: Grapheme and range model
3. Phase 3: Search, compare, and collation completion
4. Phase 4: Encoding/transcoding and data interchange
5. Phase 5: Identifier/security and diagnostics utilities
6. Phase 6: Regex and text-pipeline integration

## Phase 1: Scalar and Property Completion

### Objective

Finish the Python-`unicodedata` equivalent layer.

### New constants

```starbytes
decl imut BIDI_LTR:Int
decl imut BIDI_RTL:Int
decl imut BIDI_NEUTRAL:Int
```

### Expand `UnicodeScalarInfo`

```starbytes
class UnicodeScalarInfo {
    func codepoint() Int
    func name() String
    func category() String
    func script() String

    func block() String
    func bidiClass() String
    func combiningClass() Int
    func numericValue() Double?
    func decimalValue() Int?
    func digitValue() Int?

    func isAlphabetic() Bool
    func isDigit() Bool
    func isWhitespace() Bool
    func isUppercase() Bool
    func isLowercase() Bool
    func isTitlecase() Bool
    func isEmojiLike() Bool
    func isVariationSelector() Bool
    func isJoinControl() Bool
}
```

### New module functions

```starbytes
func scalarInfo(codepoint:Int) UnicodeScalarInfo!
func scalarName(codepoint:Int) String!
func scalarFromName(name:String) Int?
func category(codepoint:Int) String!
func script(codepoint:Int) String!
func block(codepoint:Int) String!
func bidiClass(codepoint:Int) String!
func combiningClass(codepoint:Int) Int!
func numericValue(codepoint:Int) Double?
func decimalValue(codepoint:Int) Int?
func digitValue(codepoint:Int) Int?
```

### Rationale

- This completes the practical Unicode metadata surface.
- It aligns with Python usage patterns.
- It stays value-oriented and easy to inspect from LSP hover/docs.

## Phase 2: Grapheme and Range Model

### Objective

Add the Swift-like layer that distinguishes scalar offsets from user-visible text units.

### New structs/classes

```starbytes
struct TextRange {
    decl start:Int
    decl end:Int
}

class Grapheme {
    func text() String
    func scalarCount() Int
    func codepoints() Array<Int>
    func displayWidth(locale:Locale) Int!
}

class BreakSegment {
    func text() String
    func range() TextRange
    func kind() Int
}
```

### New module functions

```starbytes
func graphemeCount(text:String,locale:Locale) Int!
func graphemeAt(text:String,index:Int,locale:Locale) String?
func graphemeSlice(text:String,start:Int,end:Int,locale:Locale) String!
func graphemeRanges(text:String,locale:Locale) Array<TextRange>!
func graphemeSegments(text:String,locale:Locale) Array<BreakSegment>!

func scalarIndexToByteOffset(text:String,index:Int) Int?
func byteOffsetToScalarIndex(text:String,offset:Int) Int?
func scalarIndexToGraphemeIndex(text:String,index:Int,locale:Locale) Int?
func graphemeIndexToScalarRange(text:String,index:Int,locale:Locale) TextRange?
```

### Break iterator completion

Replace the current low-level boundary-only experience with higher-level helpers:

```starbytes
class BreakIterator {
    func boundaries() Array<Int>!
    func segments() Array<BreakSegment>!
    func count() Int!
    func at(index:Int) BreakSegment?
}
```

### Rationale

- Current `breakIteratorCreate(...).boundaries()` is too low-level for most users.
- Starbytes already supports structs and optionals cleanly; use them.
- This is the correct bridge between builtin string scalar semantics and Unicode grapheme semantics.

## Phase 3: Search, Compare, and Collation Completion

### Objective

Finish locale-aware matching, search, and ordering.

### Extend `Collator`

```starbytes
class Collator {
    func compare(lhs:String,rhs:String) Int!
    func equals(lhs:String,rhs:String) Bool!
    func less(lhs:String,rhs:String) Bool!
    func sortKey(text:String) Array<Int>!
}
```

### Add collator options

```starbytes
struct CollatorOptions {
    decl strength:Int
    decl caseLevel:Bool
    decl numeric:Bool
    decl ignorePunctuation:Bool
}

func collatorCreateWith(locale:Locale,options:CollatorOptions) Collator!
```

### Search API

```starbytes
func find(text:String,pattern:String,locale:Locale) TextRange?
func findLast(text:String,pattern:String,locale:Locale) TextRange?
func findFolded(text:String,pattern:String,locale:Locale) TextRange?
func allFind(text:String,pattern:String,locale:Locale) Array<TextRange>!
func compare(lhs:String,rhs:String,locale:Locale) Int!
func equals(lhs:String,rhs:String,locale:Locale) Bool!
```

### Rationale

- Current folded contains/prefix/suffix helpers are useful but incomplete.
- Full text search should return ranges, not only `Bool`.
- Locale-aware compare/equality should not require explicit collator lifecycle for the simple case.

## Phase 4: Encoding and Transcoding

### Objective

Make `Unicode` the standard text-encoding and text-boundary module, not just the ICU metadata wrapper.

### New enums/constants

```starbytes
decl imut UTF8:Int
decl imut UTF16LE:Int
decl imut UTF16BE:Int
decl imut UTF32LE:Int
decl imut UTF32BE:Int
```

### New functions

```starbytes
func isValidUTF8(bytes:Array<Int>) Bool!
func decodeUTF8(bytes:Array<Int>) String!
func encodeUTF8(text:String) Array<Int>!

func transcode(text:String,encoding:Int) Array<Int>!
func decode(bytes:Array<Int>,encoding:Int) String!
func encode(text:String,encoding:Int) Array<Int>!

func scalarCountUTF8(bytes:Array<Int>) Int!
func byteOffsetsForScalars(text:String) Array<Int>!
```

### Rationale

- This is a common production need for network, FS, JSON, and archive flows.
- It lets `Unicode` become the authoritative text-boundary/encoding module instead of scattering this across `IO`, `FS`, or `Text`.

## Phase 5: Identifier, Security, and Diagnostics Utilities

### Objective

Add the security- and tooling-oriented Unicode helpers that production systems actually need.

### New functions

```starbytes
func isIdentifierStart(codepoint:Int) Bool!
func isIdentifierContinue(codepoint:Int) Bool!
func isValidIdentifier(text:String) Bool!

func skeleton(text:String,locale:Locale) String!
func confusableEquals(lhs:String,rhs:String,locale:Locale) Bool!
func stripDefaultIgnorables(text:String) String!
func hasMixedScripts(text:String) Bool!
func scriptSet(text:String) Array<String>!

func explain(text:String,locale:Locale) Dict<String,Any>!
```

### Rationale

- This supports package naming, compiler diagnostics, security checks, and editor tooling.
- The compiler and linter can consume these helpers later for identifier diagnostics and homoglyph hardening.

## Phase 6: Regex and Text-Pipeline Integration

### Objective

Connect `Unicode` with the builtin `Regex` and the `Text` ecosystem without duplicating regex functionality.

### Recommended additions

```starbytes
func regexFoldedMatch(pattern:Regex,text:String,locale:Locale) Bool!
func regexFoldedFindAll(pattern:Regex,text:String,locale:Locale) Array<String>!
func normalizeForRegex(text:String,form:Int) String!
```

### Better direction

The stronger long-term move is to teach builtin `Regex` about:

- Unicode case-fold options
- grapheme-aware matching modes
- canonical-equivalence preprocessing hooks

`Unicode` should provide the preprocessing and property/query layer, not become a second regex engine.

## Proposed Additional Types

These are worth adding once phases 1-3 are in place:

```starbytes
struct LocaleOptions {
    decl identifier:String
}

struct CaseMappingOptions {
    decl foldTurkic:Bool
}

struct BreakOptions {
    decl kind:Int
    decl skipWhitespace:Bool
}
```

## Implementation Notes by Layer

## Layer A: Runtime core

Keep in core runtime:

- UTF-8 scalar length
- scalar indexing
- scalar slicing primitives
- byte-offset conversion helpers

Reason:

- builtin `String` already depends on this
- faster and simpler than pushing basic scalar operations through ICU

## Layer B: `Unicode` native module

Use ICU for:

- normalization
- locale casing
- grapheme/word/sentence/line boundaries
- scalar properties
- bidi/script/block/category data
- collation
- transliteration and charset conversion if added

## Layer C: Compiler/LSP reuse

Planned consumers after the module is complete:

- compiler identifier validation and warning surfaces
- linter homoglyph/mixed-script diagnostics
- LSP hover and semantic token enrichment for Unicode-aware text analysis
- `starbytes-ling` code actions for normalization and confusable cleanup

## Recommended Phase Order

1. Phase 1: Scalar and property completion
2. Phase 2: Grapheme and range model
3. Phase 3: Search/collation completion
4. Phase 4: Encoding/transcoding
5. Phase 5: Identifier/security utilities
6. Phase 6: Regex/text integration

This order is intentional:

- Phase 1 finishes the lowest-risk ICU metadata layer.
- Phase 2 gives the biggest API-quality jump.
- Phase 3 completes real application-facing text comparison/search.
- Phase 4 and 5 are broader system features once the core model is stable.

## Testing Strategy

Each phase should add:

1. ICU-backed happy-path tests
2. fallback/backend-sensitive tests where behavior differs
3. invalid-case tests
4. multilingual regressions

Minimum language set for coverage:

- ASCII English
- accented Latin
- Greek
- Cyrillic
- Arabic
- CJK
- emoji and variation selectors
- combining-mark sequences

Required invariants:

- scalar indexing and scalar count stay coherent
- grapheme APIs are coherent with ICU break iteration
- locale-sensitive casing/collation differ where expected
- malformed inputs fail loudly and predictably

## First Thin Slice Recommendation

If implementation starts now, the best first slice is:

1. Phase 1 scalar/property completion
2. Phase 2 `TextRange`, `graphemeCount`, `graphemeAt`, `graphemeSlice`

Why:

- it completes the conceptual model around the new scalar-correct builtin `String`
- it gives users immediate value
- it lays the right foundation for search, regex integration, and editor tooling

## Non-Goals

Do not do these early:

- custom regex engine work inside `Unicode`
- full text layout/shaping
- terminal rendering policy beyond width estimation
- font fallback or glyph rasterization concerns

Those belong in different modules or layers.

