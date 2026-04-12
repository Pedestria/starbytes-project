Math
====

Purpose
-------

Deterministic scalar math helpers with constants, numeric predicates, and
aggregation routines.

Types and Constants
-------------------

.. code-block:: text

   def NumberList = Array<Any>

   decl imut pi:Double
   decl imut tau:Double
   decl imut e:Double
   decl imut nan:Double
   decl imut infinity:Double
   decl imut negativeInfinity:Double

Representative API
------------------

.. code-block:: text

   func sqrt(value:Any) Double!
   func abs(value:Any) Any!
   func min(lhs:Any,rhs:Any) Any!
   func max(lhs:Any,rhs:Any) Any!
   func pow(base:Any,exponent:Any) Double!
   func exp(value:Any) Double!
   func log(value:Any) Double!
   func sin(angle:Any) Double!
   func cos(angle:Any) Double!
   func tan(angle:Any) Double!
   func floor(value:Any) Double!
   func ceil(value:Any) Double!
   func round(value:Any) Double!
   func isFinite(value:Any) Bool!
   func isInfinite(value:Any) Bool!
   func isNaN(value:Any) Bool!
   func degreesToRadians(angle:Any) Double!
   func radiansToDegrees(angle:Any) Double!
   func approxEqual(lhs:Any,rhs:Any,epsilon:Any) Bool!
   func clamp(value:Any,lower:Any,upper:Any) Any!
   func gcd(lhs:Any,rhs:Any) Long!
   func lcm(lhs:Any,rhs:Any) Long!
   func isPowerOfTwo(value:Any) Bool!
   func nextPowerOfTwo(value:Any) Long!
   func sum(values:NumberList) Any!
   func product(values:NumberList) Any!
   func mean(values:NumberList) Double!
   func variance(values:NumberList) Double!
   func stddev(values:NumberList) Double!

Notes
-----

* The surface accepts ``Any`` for many numeric inputs and validates at runtime.
* Aggregate helpers operate over ``Array<Any>`` values that are expected to be
  numeric.
