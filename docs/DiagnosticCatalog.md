# Starbytes Diagnostic Catalog

## Purpose

This document inventories the current Starbytes diagnostic surface and proposes a concrete numbering layout so diagnostics can be:

- distinguished from each other
- searched reliably
- reviewed in one place
- promoted from message-only text to stable IDs

## Current State

Current diagnostic behavior is inconsistent across subsystems.

- Parser has one explicit stable code in [`src/compiler/Parser.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/Parser.cpp).
- Lint rules already have stable codes in [`src/linguistics/LintEngine.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/LintEngine.cpp).
- Semantic diagnostics currently collapse to generic buckets in [`src/compiler/SemanticA.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp):
  - errors: `SB-SEMA-E0001`
  - warnings: `SB-SEMA-W0001`
- Many runtime/tool diagnostics are free-text only and get hash-derived fallback codes through [`src/base/Diagnostic.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/base/Diagnostic.cpp).

Important consequence:

- the current emitted `code` field is not a reliable one-to-one identifier for most semantic and runtime diagnostics
- this catalog therefore distinguishes:
  - existing stable codes
  - proposed split codes for currently lumped diagnostics

## Coverage Summary

| Area | Stable today | Cataloged here | Notes |
|---|---:|---:|---|
| Parser | 1 | 2 | includes lexer malformed-numeric case |
| General/tool | 0 | 1 | disassembler file-read failure |
| Semantic literal messages | 0 distinct | 135 | currently emitted as generic `SB-SEMA-E0001/W0001` |
| Semantic dynamic-message callsites | 0 distinct | summarized only | 70 callsites still need dedicated IDs |
| Runtime literal/free-text messages | 0 distinct | 15 | currently emitted as free-text/hash-derived codes |
| Runtime dynamic-message paths | 0 distinct | summarized only | native/runtime pass-through errors |
| Lint | 6 | 6 | already stable |

## Proposed Numbering Policy

- `SB-PARSE-E/Wxxxx`
  - parser and lexer diagnostics
- `SB-SEMA-E/Wxxxx`
  - semantic analyzer diagnostics
- `SB-RUNTIME-E/Wxxxx`
  - interpreter/runtime diagnostics
- `SB-GENERAL-E/Wxxxx`
  - tool or non-phase-specific diagnostics
- `SB-LINT-*`
  - keep current lint rule codes as-is

## Parser, Lexer, and General Diagnostics

| Severity | Code | Description | Source |
|---|---|---|---|
| error | `SB-PARSE-E0001` | Unexpected token ``<token>``. | [`src/compiler/Parser.cpp:286`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/Parser.cpp#L286) |
| error | `SB-PARSE-E0002` | Malformed numeric literal ``<literal>``. | [`src/compiler/Lexer.cpp:132`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/Lexer.cpp#L132) |
| error | `SB-GENERAL-E0001` | Failed to read file. | [`tools/disasm/disasm.cpp:38`](/Users/alextopper/Documents/GitHub/starbytes-project/tools/disasm/disasm.cpp#L38) |

## Runtime Diagnostics

These are proposed stable IDs for current explicit runtime error strings.

| Severity | Proposed code | Description | Source |
|---|---|---|---|
| error | `SB-RUNTIME-E0001` | `sqrt` requires non-negative numeric input. | [`src/runtime/RTEngine.cpp:1549`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L1549) |
| error | `SB-RUNTIME-E0002` | Native callback ``<name>`` is not loaded. | [`src/runtime/RTEngine.cpp:1763`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L1763) |
| error | `SB-RUNTIME-E0003` | Invocation target is not a function. | [`src/runtime/RTEngine.cpp:2130`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2130) |
| error | `SB-RUNTIME-E0004` | ``await`` requires a `Task` value. | [`src/runtime/RTEngine.cpp:2218`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2218) |
| error | `SB-RUNTIME-E0005` | `await` stalled on unresolved task. | [`src/runtime/RTEngine.cpp:2223`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2223) |
| error | `SB-RUNTIME-E0006` | Task rejected. | [`src/runtime/RTEngine.cpp:2239`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2239) |
| error | `SB-RUNTIME-E0007` | Ternary condition must be `Bool`. | [`src/runtime/RTEngine.cpp:2958`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2958) |
| error | `SB-RUNTIME-E0008` | `String.at` index out of range. | [`src/runtime/RTEngine.cpp:3012`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3012) |
| error | `SB-RUNTIME-E0009` | Cannot assign through `String` index; `String` is immutable. | [`src/runtime/RTEngine.cpp:3107`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3107) |
| error | `SB-RUNTIME-E0010` | Failed to allocate class instance. | [`src/runtime/RTEngine.cpp:3191`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3191) |
| error | `SB-RUNTIME-E0011` | Builtin metadata member ``<member>`` is read-only. | [`src/runtime/RTEngine.cpp:3325`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3325) |
| error | `SB-RUNTIME-E0012` | Regex compile error. | [`src/runtime/RTEngine.cpp:4086`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4086) |
| error | `SB-RUNTIME-E0013` | Native global ``<name>`` is not loaded. | [`src/runtime/RTEngine.cpp:4524`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4524) |
| error | `SB-RUNTIME-E0014` | Native global ``<name>`` returned no value. | [`src/runtime/RTEngine.cpp:4530`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4530) |
| error | `SB-RUNTIME-E0015` | Failed to load native module ``<path>``. | [`src/runtime/RTEngine.cpp:4846`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4846) |

## Lint Diagnostics

These codes already exist and are stable.

| Severity | Code | Description | Source |
|---|---|---|---|
| warning | `SB-LINT-STYLE-S0001` | Line has trailing whitespace. | [`src/linguistics/LintEngine.cpp:128`](/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/LintEngine.cpp#L128) |
| warning | `SB-LINT-CORR-C0001` | Condition contains an assignment expression. | [`src/linguistics/LintEngine.cpp:136`](/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/LintEngine.cpp#L136) |
| information | `SB-LINT-PERF-P0001` | Object allocation appears inside a loop body. | [`src/linguistics/LintEngine.cpp:144`](/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/LintEngine.cpp#L144) |
| warning | `SB-LINT-SAFE-A0001` | Catch clause is missing an explicit typed error binding. | [`src/linguistics/LintEngine.cpp:152`](/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/LintEngine.cpp#L152) |
| information | `SB-LINT-DOC-D0001` | Top-level declaration is missing a leading documentation comment. | [`src/linguistics/LintEngine.cpp:160`](/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/LintEngine.cpp#L160) |
| information | `SB-LINT-CORR-C0004` | Declaration shadows an outer binding. | [`src/linguistics/LintEngine.cpp:168`](/Users/alextopper/Documents/GitHub/starbytes-project/src/linguistics/LintEngine.cpp#L168) |

## Semantic Literal Diagnostics

Current implementation note:

- every semantic error emitted through `SemanticADiagnostic::create(...)` is currently stamped as `SB-SEMA-E0001`
- every semantic warning emitted through the same path is currently stamped as `SB-SEMA-W0001`
- the rows below are the proposed split IDs for the literal semantic diagnostics that can already be enumerated directly from source

| Severity | Proposed code | Description | Source |
|---|---|---|---|
| error | `SB-SEMA-E0001` | Optional or throwable values must be captured with a secure declaration. | [`src/compiler/DeclSema.cpp:132`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L132) |
| error | `SB-SEMA-E0002` | Context: Expression was evaluated in as a conditional specifier | [`src/compiler/DeclSema.cpp:156`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L156) |
| error | `SB-SEMA-E0003` | Malformed loop declaration. | [`src/compiler/DeclSema.cpp:224`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L224) |
| error | `SB-SEMA-E0004` | Context: Expression was evaluated in as a loop condition | [`src/compiler/DeclSema.cpp:236`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L236) |
| error | `SB-SEMA-E0005` | Malformed secure declaration. | [`src/compiler/DeclSema.cpp:277`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L277) |
| error | `SB-SEMA-E0006` | Secure declaration requires an initializer expression. | [`src/compiler/DeclSema.cpp:291`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L291) |
| error | `SB-SEMA-E0007` | Secure declaration requires an optional or throwable initializer. | [`src/compiler/DeclSema.cpp:302`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L302) |
| error | `SB-SEMA-E0008` | Unknown catch error type in secure declaration. | [`src/compiler/DeclSema.cpp:308`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L308) |
| error | `SB-SEMA-E0009` | Cannot declare a return outside of a func scope. | [`src/compiler/DeclSema.cpp:424`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L424) |
| error | `SB-SEMA-E0010` | Runtime `is` does not support interface types. | [`src/compiler/ExprSema.cpp:891`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L891) |
| error | `SB-SEMA-E0011` | Type alias symbol is missing target type. | [`src/compiler/ExprSema.cpp:903`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L903) |
| error | `SB-SEMA-E0012` | Right side of `is` must be a runtime type. | [`src/compiler/ExprSema.cpp:908`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L908) |
| error | `SB-SEMA-E0013` | Binary expression is missing operator. | [`src/compiler/ExprSema.cpp:1408`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1408) |
| error | `SB-SEMA-E0014` | Arithmetic operators require numeric operands (or String concatenation for `+`). | [`src/compiler/ExprSema.cpp:1425`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1425) |
| error | `SB-SEMA-E0015` | Bitwise and shift operators require Int operands. | [`src/compiler/ExprSema.cpp:1435`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1435) |
| error | `SB-SEMA-E0016` | Equality comparison requires matching operand types. | [`src/compiler/ExprSema.cpp:1446`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1446) |
| error | `SB-SEMA-E0017` | Relational comparison requires numeric or string operands. | [`src/compiler/ExprSema.cpp:1455`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1455) |
| error | `SB-SEMA-E0018` | Logical operators require Bool operands. | [`src/compiler/ExprSema.cpp:1463`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1463) |
| error | `SB-SEMA-E0019` | Unsupported binary operator. | [`src/compiler/ExprSema.cpp:1467`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1467) |
| error | `SB-SEMA-E0020` | Failed to materialize generic default type argument. | [`src/compiler/ExprSema.cpp:1590`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1590) |
| error | `SB-SEMA-E0021` | Failed to resolve generic default type argument. | [`src/compiler/ExprSema.cpp:1598`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1598) |
| error | `SB-SEMA-E0022` | Malformed generic parameter declaration. | [`src/compiler/ExprSema.cpp:1701`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L1701) |
| error | `SB-SEMA-E0023` | Identifier in this context cannot identify any other symbol type except another variable. | [`src/compiler/ExprSema.cpp:2002`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2002) |
| error | `SB-SEMA-E0024` | Dictionary keys must be String/Int/Long/Float/Double. | [`src/compiler/ExprSema.cpp:2041`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2041) |
| error | `SB-SEMA-E0025` | Inline function is missing a return type or body. | [`src/compiler/ExprSema.cpp:2080`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2080) |
| error | `SB-SEMA-E0026` | Inline function parameter is invalid. | [`src/compiler/ExprSema.cpp:2089`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2089) |
| error | `SB-SEMA-E0027` | Malformed index expression. | [`src/compiler/ExprSema.cpp:2157`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2157) |
| error | `SB-SEMA-E0028` | Array indexing requires Int index. | [`src/compiler/ExprSema.cpp:2169`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2169) |
| error | `SB-SEMA-E0029` | String indexing requires Int index. | [`src/compiler/ExprSema.cpp:2177`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2177) |
| error | `SB-SEMA-E0030` | Dictionary indexing requires String/Int/Long/Float/Double key. | [`src/compiler/ExprSema.cpp:2185`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2185) |
| error | `SB-SEMA-E0031` | Map indexing key type mismatch. | [`src/compiler/ExprSema.cpp:2197`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2197) |
| error | `SB-SEMA-E0032` | Index expression requires Array, Dict, or Map base. | [`src/compiler/ExprSema.cpp:2204`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2204) |
| error | `SB-SEMA-E0033` | Malformed unary expression. | [`src/compiler/ExprSema.cpp:2209`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2209) |
| error | `SB-SEMA-E0034` | Unary `+` requires numeric operand. | [`src/compiler/ExprSema.cpp:2220`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2220) |
| error | `SB-SEMA-E0035` | Unary `-` requires numeric operand. | [`src/compiler/ExprSema.cpp:2228`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2228) |
| error | `SB-SEMA-E0036` | Unary `!` requires Bool operand. | [`src/compiler/ExprSema.cpp:2236`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2236) |
| error | `SB-SEMA-E0037` | Unary `~` requires Int operand. | [`src/compiler/ExprSema.cpp:2244`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2244) |
| error | `SB-SEMA-E0038` | `await` requires a Task value. | [`src/compiler/ExprSema.cpp:2252`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2252) |
| error | `SB-SEMA-E0039` | Task type must include exactly one type argument for `await`. | [`src/compiler/ExprSema.cpp:2256`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2256) |
| error | `SB-SEMA-E0040` | Unsupported unary operator. | [`src/compiler/ExprSema.cpp:2262`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2262) |
| error | `SB-SEMA-E0041` | Malformed binary expression. | [`src/compiler/ExprSema.cpp:2267`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2267) |
| error | `SB-SEMA-E0042` | Right side of `is` must be a type. | [`src/compiler/ExprSema.cpp:2325`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2325) |
| error | `SB-SEMA-E0043` | Right side of `is` must be a class or builtin type. | [`src/compiler/ExprSema.cpp:2338`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2338) |
| error | `SB-SEMA-E0044` | Right side of `is` must be a type identifier. | [`src/compiler/ExprSema.cpp:2349`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2349) |
| error | `SB-SEMA-E0045` | Malformed ternary expression. | [`src/compiler/ExprSema.cpp:2375`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2375) |
| error | `SB-SEMA-E0046` | Ternary condition must be Bool. | [`src/compiler/ExprSema.cpp:2385`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2385) |
| error | `SB-SEMA-E0047` | Ternary branch type mismatch. | [`src/compiler/ExprSema.cpp:2419`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2419) |
| error | `SB-SEMA-E0048` | Malformed member expression. | [`src/compiler/ExprSema.cpp:2424`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2424) |
| error | `SB-SEMA-E0049` | Unknown symbol in scope member access. | [`src/compiler/ExprSema.cpp:2463`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2463) |
| error | `SB-SEMA-E0050` | Unsupported scope member type. | [`src/compiler/ExprSema.cpp:2496`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2496) |
| error | `SB-SEMA-E0051` | Unknown String member. | [`src/compiler/ExprSema.cpp:2522`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2522) |
| error | `SB-SEMA-E0052` | Unknown Regex member. | [`src/compiler/ExprSema.cpp:2530`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2530) |
| error | `SB-SEMA-E0053` | Unknown Array member. | [`src/compiler/ExprSema.cpp:2546`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2546) |
| error | `SB-SEMA-E0054` | Unknown Dict member. | [`src/compiler/ExprSema.cpp:2560`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2560) |
| error | `SB-SEMA-E0055` | Unknown Map member. | [`src/compiler/ExprSema.cpp:2574`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2574) |
| error | `SB-SEMA-E0056` | Unknown class member. | [`src/compiler/ExprSema.cpp:2734`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2734) |
| error | `SB-SEMA-E0057` | Malformed assignment expression. | [`src/compiler/ExprSema.cpp:2739`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2739) |
| error | `SB-SEMA-E0058` | Unsupported compound assignment operator. | [`src/compiler/ExprSema.cpp:2785`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2785) |
| error | `SB-SEMA-E0059` | Compound assignment result type mismatch. | [`src/compiler/ExprSema.cpp:2795`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2795) |
| error | `SB-SEMA-E0060` | Cannot assign to const variable. | [`src/compiler/ExprSema.cpp:2811`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2811) |
| error | `SB-SEMA-E0061` | Assignment to scope-qualified symbols is not supported. | [`src/compiler/ExprSema.cpp:2819`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2819) |
| error | `SB-SEMA-E0062` | Cannot assign to readonly/const field. | [`src/compiler/ExprSema.cpp:2833`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2833) |
| error | `SB-SEMA-E0063` | Array assignment index must be Int. | [`src/compiler/ExprSema.cpp:2872`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2872) |
| error | `SB-SEMA-E0064` | Cannot assign through String index; String is immutable. | [`src/compiler/ExprSema.cpp:2877`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2877) |
| error | `SB-SEMA-E0065` | Dictionary assignment key must be String/Int/Long/Float/Double. | [`src/compiler/ExprSema.cpp:2882`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2882) |
| error | `SB-SEMA-E0066` | Map assignment key type mismatch. | [`src/compiler/ExprSema.cpp:2892`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2892) |
| error | `SB-SEMA-E0067` | Assignment target is not indexable. | [`src/compiler/ExprSema.cpp:2898`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2898) |
| error | `SB-SEMA-E0068` | Unsupported assignment target. | [`src/compiler/ExprSema.cpp:2903`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2903) |
| error | `SB-SEMA-E0069` | Cast target type is invalid. | [`src/compiler/ExprSema.cpp:2918`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2918) |
| error | `SB-SEMA-E0070` | Cast invocation does not support generic type arguments. | [`src/compiler/ExprSema.cpp:2922`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2922) |
| error | `SB-SEMA-E0071` | Type cast expects exactly one argument. | [`src/compiler/ExprSema.cpp:2926`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2926) |
| error | `SB-SEMA-E0072` | Cannot cast values to Void. | [`src/compiler/ExprSema.cpp:2939`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2939) |
| error | `SB-SEMA-E0073` | Invalid builtin conversion for cast target type. | [`src/compiler/ExprSema.cpp:2968`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2968) |
| error | `SB-SEMA-E0074` | Casting to interface types is not currently supported. | [`src/compiler/ExprSema.cpp:2981`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2981) |
| error | `SB-SEMA-E0075` | Cast target must resolve to a builtin or class type. | [`src/compiler/ExprSema.cpp:2985`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2985) |
| error | `SB-SEMA-E0076` | Class casts require a class-typed source value. | [`src/compiler/ExprSema.cpp:2994`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L2994) |
| error | `SB-SEMA-E0077` | Class cast source and target types are unrelated. | [`src/compiler/ExprSema.cpp:3007`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3007) |
| error | `SB-SEMA-E0078` | Malformed function type. | [`src/compiler/ExprSema.cpp:3330`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3330) |
| error | `SB-SEMA-E0079` | Incorrect number of function arguments. | [`src/compiler/ExprSema.cpp:3335`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3335) |
| error | `SB-SEMA-E0080` | Malformed function parameter type. | [`src/compiler/ExprSema.cpp:3346`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3346) |
| error | `SB-SEMA-E0081` | Malformed function return type. | [`src/compiler/ExprSema.cpp:3355`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3355) |
| error | `SB-SEMA-E0082` | Scope member invocation requires a function symbol. | [`src/compiler/ExprSema.cpp:3370`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3370) |
| error | `SB-SEMA-E0083` | Incorrect number of method arguments. | [`src/compiler/ExprSema.cpp:3406`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3406) |
| error | `SB-SEMA-E0084` | Method argument type mismatch. | [`src/compiler/ExprSema.cpp:3427`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3427) |
| error | `SB-SEMA-E0085` | Unknown String method. | [`src/compiler/ExprSema.cpp:3551`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3551) |
| error | `SB-SEMA-E0086` | Unknown Regex method. | [`src/compiler/ExprSema.cpp:3574`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3574) |
| error | `SB-SEMA-E0087` | Unknown Array method. | [`src/compiler/ExprSema.cpp:3643`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3643) |
| error | `SB-SEMA-E0088` | Unknown Dict method. | [`src/compiler/ExprSema.cpp:3687`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3687) |
| error | `SB-SEMA-E0089` | Unknown Map method. | [`src/compiler/ExprSema.cpp:3736`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3736) |
| error | `SB-SEMA-E0090` | Unknown method. | [`src/compiler/ExprSema.cpp:3747`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3747) |
| error | `SB-SEMA-E0091` | Invalid generic type argument in constructor call. | [`src/compiler/ExprSema.cpp:3867`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3867) |
| error | `SB-SEMA-E0092` | No constructor matches argument count. | [`src/compiler/ExprSema.cpp:3887`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L3887) |
| error | `SB-SEMA-E0093` | Constructor call requires a class type. | [`src/compiler/ExprSema.cpp:4010`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L4010) |
| error | `SB-SEMA-E0094` | Incorrect number of arguments | [`src/compiler/ExprSema.cpp:4041`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L4041) |
| error | `SB-SEMA-E0095` | sqrt expects exactly one argument. | [`src/compiler/ExprSema.cpp:4058`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L4058) |
| error | `SB-SEMA-E0096` | sqrt requires a numeric argument. | [`src/compiler/ExprSema.cpp:4067`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L4067) |
| error | `SB-SEMA-E0097` | abs expects exactly one argument. | [`src/compiler/ExprSema.cpp:4074`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L4074) |
| error | `SB-SEMA-E0098` | abs requires a numeric argument. | [`src/compiler/ExprSema.cpp:4083`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L4083) |
| error | `SB-SEMA-E0099` | Generic free-function invocation requires explicit type arguments. | [`src/compiler/ExprSema.cpp:4130`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp#L4130) |
| error | `SB-SEMA-E0100` | Malformed class method generic parameter. | [`src/compiler/SemanticA.cpp:2657`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L2657) |
| error | `SB-SEMA-E0101` | Superclass override contract is missing return type metadata. | [`src/compiler/SemanticA.cpp:2667`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L2667) |
| error | `SB-SEMA-E0102` | Generic type parameters cannot have nested type arguments. | [`src/compiler/SemanticA.cpp:3638`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L3638) |
| error | `SB-SEMA-E0103` | Type `Task` expects exactly one type argument. | [`src/compiler/SemanticA.cpp:3646`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L3646) |
| error | `SB-SEMA-E0104` | Type `Map` expects exactly two type arguments. | [`src/compiler/SemanticA.cpp:3654`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L3654) |
| error | `SB-SEMA-E0105` | Type `Map` key must be String/Int/Long/Float/Double (or generic). | [`src/compiler/SemanticA.cpp:3666`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L3666) |
| error | `SB-SEMA-E0106` | Function type must include a return type. | [`src/compiler/SemanticA.cpp:3674`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L3674) |
| error | `SB-SEMA-E0107` | Generic parameters with defaults must be trailing. | [`src/compiler/SemanticA.cpp:4058`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4058) |
| error | `SB-SEMA-E0108` | Function declaration without a body must be marked @native. | [`src/compiler/SemanticA.cpp:4106`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4106) |
| error | `SB-SEMA-E0109` | Native function declaration without a body must declare a return type. | [`src/compiler/SemanticA.cpp:4110`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4110) |
| error | `SB-SEMA-E0110` | Function declaration is missing a body. | [`src/compiler/SemanticA.cpp:4116`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4116) |
| error | `SB-SEMA-E0111` | Invalid type in class inheritance list. | [`src/compiler/SemanticA.cpp:4186`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4186) |
| error | `SB-SEMA-E0112` | Unknown type in class inheritance list. | [`src/compiler/SemanticA.cpp:4195`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4195) |
| error | `SB-SEMA-E0113` | Class can only extend one superclass. | [`src/compiler/SemanticA.cpp:4200`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4200) |
| error | `SB-SEMA-E0114` | Superclass symbol is missing metadata. | [`src/compiler/SemanticA.cpp:4205`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4205) |
| error | `SB-SEMA-E0115` | Interface symbol is missing metadata. | [`src/compiler/SemanticA.cpp:4220`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4220) |
| error | `SB-SEMA-E0116` | Only class or interface types are allowed in class inheritance list. | [`src/compiler/SemanticA.cpp:4233`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4233) |
| error | `SB-SEMA-E0117` | Circular class inheritance detected. | [`src/compiler/SemanticA.cpp:4246`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4246) |
| error | `SB-SEMA-E0118` | Duplicate class method name. | [`src/compiler/SemanticA.cpp:4277`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4277) |
| error | `SB-SEMA-E0119` | Class method declaration without a body must be marked @native. | [`src/compiler/SemanticA.cpp:4302`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4302) |
| error | `SB-SEMA-E0120` | Native class method declaration without a body must declare a return type. | [`src/compiler/SemanticA.cpp:4306`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4306) |
| error | `SB-SEMA-E0121` | Class method declaration is missing a body. | [`src/compiler/SemanticA.cpp:4312`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4312) |
| error | `SB-SEMA-E0122` | Duplicate constructor arity. | [`src/compiler/SemanticA.cpp:4365`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4365) |
| error | `SB-SEMA-E0123` | Constructor cannot return a value. | [`src/compiler/SemanticA.cpp:4396`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4396) |
| error | `SB-SEMA-E0124` | Invalid interface in class implements list. | [`src/compiler/SemanticA.cpp:4415`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4415) |
| error | `SB-SEMA-E0125` | Interface method return type could not be resolved. | [`src/compiler/SemanticA.cpp:4516`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4516) |
| error | `SB-SEMA-E0126` | Duplicate interface name in current scope. | [`src/compiler/SemanticA.cpp:4556`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4556) |
| error | `SB-SEMA-E0127` | Interface method is missing an identifier. | [`src/compiler/SemanticA.cpp:4583`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4583) |
| error | `SB-SEMA-E0128` | Duplicate interface method name. | [`src/compiler/SemanticA.cpp:4587`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4587) |
| error | `SB-SEMA-E0129` | Interface method declaration without a body must declare a return type. | [`src/compiler/SemanticA.cpp:4614`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4614) |
| error | `SB-SEMA-E0130` | Duplicate type alias name in current scope. | [`src/compiler/SemanticA.cpp:4669`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4669) |
| error | `SB-SEMA-E0131` | Type alias is missing target type. | [`src/compiler/SemanticA.cpp:4677`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4677) |
| error | `SB-SEMA-E0132` | Import declaration is missing a module name. | [`src/compiler/SemanticA.cpp:4692`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4692) |
| error | `SB-SEMA-E0133` | Duplicate scope name in current scope. | [`src/compiler/SemanticA.cpp:4703`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4703) |
| error | `SB-SEMA-E0134` | Return can not be declared in a namespace scope | [`src/compiler/SemanticA.cpp:4725`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp#L4725) |
| warning | `SB-SEMA-W0001` | The code here and below is unreachable. | [`src/compiler/DeclSema.cpp:385`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp#L385) |

## Dynamic Diagnostic Families Still Missing Stable IDs

These are not exhaustively listed as one row per diagnostic because the current implementation formats them dynamically at the call site and then emits generic or hash-derived codes.

### Semantic dynamic callsites

Current breakdown:

- [`src/compiler/DeclSema.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/DeclSema.cpp): 5 dynamic semantic diagnostic callsites
- [`src/compiler/ExprSema.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp): 23 dynamic semantic diagnostic callsites
- [`src/compiler/SemanticA.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SemanticA.cpp): 40 dynamic semantic diagnostic callsites
- [`src/compiler/SymTable.cpp`](/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/SymTable.cpp): 2 dynamic semantic diagnostic callsites

These should be promoted to dedicated numbered diagnostics next. Common patterns include:

- duplicate symbol/type names with interpolated identifiers
- type mismatch messages that interpolate expected and actual types
- deprecation warnings with symbol names
- constructor and inheritance contract mismatches with interpolated class/interface names
- generic-resolution failures that currently render through `ss.str()`, `out.str()`, `res`, or `message`

### Runtime dynamic-message paths

Current runtime error paths that still need dedicated IDs include:

- `nativeError` pass-through from native callbacks
- task rejection reason propagation via `reason`
- regex engine message propagation from the PCRE2 error buffer
- dynamic member/native/global names embedded in runtime errors
- generic runtime message forwarding through local `message` / `error` variables

Relevant locations include:

- [`src/runtime/RTEngine.cpp:1689`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L1689)
- [`src/runtime/RTEngine.cpp:1730`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L1730)
- [`src/runtime/RTEngine.cpp:2236`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2236)
- [`src/runtime/RTEngine.cpp:2739`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L2739)
- [`src/runtime/RTEngine.cpp:3435`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3435)
- [`src/runtime/RTEngine.cpp:3647`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3647)
- [`src/runtime/RTEngine.cpp:3667`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3667)
- [`src/runtime/RTEngine.cpp:3688`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L3688)
- [`src/runtime/RTEngine.cpp:4090`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4090)
- [`src/runtime/RTEngine.cpp:4093`](/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTEngine.cpp#L4093)

## Recommended Next Step

After this catalog, the next implementation step should be:

1. replace generic semantic stamping in `SemanticADiagnostic::create(...)` with per-diagnostic codes
2. replace runtime free-text/hash-derived errors with explicit `SB-RUNTIME-Exxxx` assignments at the emission sites
3. keep lint codes unchanged
4. add a regression test that asserts the emitted codes for a representative subset of parser, semantic, runtime, and lint diagnostics
