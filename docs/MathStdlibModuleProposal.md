# Math Stdlib Module Proposal

Last updated: March 6, 2026

## Purpose
Define a `Math` stdlib module that completes Starbytes math capabilities without breaking existing intrinsic globals or duplicating unrelated modules such as `Random` and `Time`.

## Current Baseline
Starbytes already exposes these global intrinsic math functions:
- `sqrt(value:Number) Double`
- `abs(value:Number) Number`
- `min(lhs:Number,rhs:Number) Number`
- `max(lhs:Number,rhs:Number) Number`

Their runtime behavior now lives in:
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTStdlib.cpp`

Their semantic signatures remain in:
- `/Users/alextopper/Documents/GitHub/starbytes-project/src/compiler/ExprSema.cpp`
- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/builtins.starbint`

## Responsibility Split

### Keep Intrinsic
These should remain language-level globals for bootstrap ergonomics and because they are fundamental enough to justify direct lowering:
- `sqrt`
- `abs`
- `min`
- `max`

### Add To `Math`
`Math` should provide the complete deterministic scalar math surface and re-export wrappers for the intrinsic globals so users can write either style:
- `sqrt(x)`
- `Math.sqrt(x)`

This keeps old code stable while giving the stdlib a coherent namespace.

### Keep Out Of `Math`
- Random number generation and distributions: `Random`
- Time/date arithmetic: `Time`
- Big integer / decimal arbitrary precision: future `BigNum` or `Decimal`
- Matrix-heavy linear algebra and FFT: future `Math.Linear` or `Numerics`

## Proposed API Matrix

| Area | API | Return | Notes |
|---|---|---|---|
| Constants | `pi`, `tau`, `e` | `Double` | Core transcendental constants |
| Constants | `nan`, `infinity`, `negativeInfinity` | `Double` | IEEE-754 aligned |
| Existing intrinsics | `sqrt`, `abs`, `min`, `max` | mixed | Re-export through `Math` |
| Powers/logs | `pow`, `exp`, `exp2`, `log`, `log2`, `log10` | `Double` | Scalar numeric foundation |
| Roots/magnitude | `cbrt`, `hypot` | `Double` | Important for numerics and geometry |
| Trig | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2` | `Double` | Expected baseline math surface |
| Hyperbolic | `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh` | `Double` | Completes scalar transcendental coverage |
| Rounding | `floor`, `ceil`, `round`, `trunc` | `Double` | Match Python/Swift expectations |
| Rounding | `roundInt`, `floorInt`, `ceilInt`, `truncInt` | `Long` | Explicit integral conversion helpers |
| Classification | `isFinite`, `isInfinite`, `isNaN`, `signbit` | `Bool` | Required for robust numeric code |
| Comparison | `approxEqual(lhs,rhs,epsilon)` | `Bool` | Practical floating-point comparison |
| Angle conversion | `degreesToRadians`, `radiansToDegrees` | `Double` | Common ergonomic helpers |
| Clamp/interp | `clamp`, `lerp` | `Number` / `Double` | Core utility math |
| Integer math | `gcd`, `lcm` | `Long` | Integer-only operations |
| Integer math | `isPowerOfTwo`, `nextPowerOfTwo` | `Bool` / `Long` | Useful for systems work |
| Aggregate helpers | `sum`, `product`, `mean` | `Number` / `Double` | Accept `Array<Number>` |
| Aggregate helpers | `variance`, `stddev` | `Double` | Useful numeric/statistics baseline |

## Proposed Interface Shape

```starbytes
scope Math {
    decl pi:Double
    decl tau:Double
    decl e:Double
    decl nan:Double
    decl infinity:Double
    decl negativeInfinity:Double

    func sqrt(value:Number) Double
    func abs(value:Number) Number
    func min(lhs:Number,rhs:Number) Number
    func max(lhs:Number,rhs:Number) Number

    func pow(base:Number, exponent:Number) Double
    func exp(value:Number) Double
    func exp2(value:Number) Double
    func log(value:Number) Double
    func log2(value:Number) Double
    func log10(value:Number) Double

    func sin(angle:Number) Double
    func cos(angle:Number) Double
    func tan(angle:Number) Double
    func asin(value:Number) Double
    func acos(value:Number) Double
    func atan(value:Number) Double
    func atan2(y:Number, x:Number) Double

    func floor(value:Number) Double
    func ceil(value:Number) Double
    func round(value:Number) Double
    func trunc(value:Number) Double

    func isFinite(value:Number) Bool
    func isInfinite(value:Number) Bool
    func isNaN(value:Number) Bool
    func approxEqual(lhs:Number, rhs:Number, epsilon:Double = 1.0e-9) Bool

    func gcd(lhs:Long, rhs:Long) Long
    func lcm(lhs:Long, rhs:Long) Long
    func clamp(value:Number, lower:Number, upper:Number) Number
    func lerp(start:Number, end:Number, t:Number) Double

    func sum(values:Number[]) Number
    func product(values:Number[]) Number
    func mean(values:Number[]) Double
    func variance(values:Number[]) Double
    func stddev(values:Number[]) Double
}
```

## Semantic Rules
- Scalar transcendental functions always return `Double`.
- `abs`, `min`, `max`, and `clamp` preserve the widest participating numeric kind when representable.
- Integer-only helpers (`gcd`, `lcm`, power-of-two helpers) require `Long` or `Int` inputs and should normalize to `Long`.
- `approxEqual` must reject negative `epsilon`.
- `variance` and `stddev` should use a numerically stable algorithm such as Welford's online method.
- Aggregate helpers on empty arrays should fail loudly unless a neutral identity is mathematically obvious:
  - `sum([]) -> 0`
  - `product([]) -> 1`
  - `mean([])` / `variance([])` / `stddev([])` -> runtime error

## Implementation Plan

### Phase 1: Namespace Completion
- Add `stdlib/Math/Math.cpp` and `stdlib/Math/Math.starbint`.
- Re-export wrappers for intrinsic `sqrt`, `abs`, `min`, and `max`.
- Add constants:
  - `pi`, `tau`, `e`, `nan`, `infinity`, `negativeInfinity`
- Add scalar baseline functions:
  - `pow`, `exp`, `log`, `log10`, `sin`, `cos`, `tan`, `floor`, `ceil`, `round`, `trunc`, `isFinite`, `isInfinite`, `isNaN`

### Phase 2: Numeric Utility Completion
- Add `atan2`, `exp2`, `log2`, `cbrt`, `hypot`, angle conversion, `approxEqual`, `clamp`, `lerp`.
- Add integer helpers:
  - `gcd`, `lcm`, `isPowerOfTwo`, `nextPowerOfTwo`

### Phase 3: Aggregate Math
- Add array-based helpers:
  - `sum`, `product`, `mean`, `variance`, `stddev`
- Use stable accumulation for floating-point arrays.
- Specialize integer-only fast paths later if profiling justifies it.

### Phase 4: Extended Numerics (Separate Surface)
Do not overload the base `Math` module with these immediately. Add only if language demand is real.
- `Math.Linear`: vectors, matrices, dot products, norms
- `Math.Complex`: complex numbers
- `Math.Special`: gamma/erf/bessel-style functions
- `Decimal` / `BigNum`: exact or arbitrary-precision arithmetic

## Runtime Strategy
- Implement the scalar `Math` module in native C++ using `<cmath>`.
- Reuse the same numeric helpers now centralized in `/Users/alextopper/Documents/GitHub/starbytes-project/src/runtime/RTStdlib.cpp` so intrinsic and module behavior cannot drift.
- Keep intrinsic globals lowered directly for bootstrap and performance.
- Implement `Math.*` wrappers either as:
  - direct native module callbacks, or
  - thin wrappers that dispatch to the same runtime helper functions.

## Recommendation
Ship `Math` in three slices:
1. Constants + scalar baseline functions
2. classification/comparison/integer helpers
3. aggregate numeric helpers

That gives Starbytes a complete practical math surface quickly, without prematurely committing to a full scientific-computing package in the base stdlib.
