Time
====

Purpose
-------

Python-style utility surface with Swift-like value semantics for durations,
instants, time zones, and date-times.

Core Types
----------

.. code-block:: text

   class Duration
   class Instant
   class TimeZone
   class DateTime

API Surface
-----------

.. code-block:: text

   func durationZero() Duration
   func durationFromNanos(nanos:Int) Duration!
   func durationFromMillis(millis:Int) Duration!
   func durationFromSeconds(seconds:Float) Duration!
   func durationAdd(lhs:Duration,rhs:Duration) Duration!
   func durationSub(lhs:Duration,rhs:Duration) Duration!
   func durationMul(duration:Duration,factor:Float) Duration!
   func durationDiv(duration:Duration,divisor:Float) Duration!
   func durationCompare(lhs:Duration,rhs:Duration) Int!
   func sleep(duration:Duration) Bool!
   func monotonicNow() Instant!
   func elapsedSince(start:Instant) Duration!
   func utcNow() DateTime!
   func localNow() DateTime!
   func timezoneUTC() TimeZone!
   func timezoneLocal() TimeZone!
   func timezoneFromID(id:String) TimeZone!
   func parseISO8601(text:String) DateTime!
   func formatISO8601(dt:DateTime,tz:TimeZone) String!
   func fromUnix(seconds:Int,nanos:Int,tz:TimeZone) DateTime!
   func convertTimeZone(dt:DateTime,tz:TimeZone) DateTime!

Representative Members
----------------------

* ``Duration`` exposes nanoseconds, milliseconds, and floating seconds.
* ``Instant`` exposes monotonic ticks.
* ``TimeZone`` exposes identifier and UTC offset minutes.
* ``DateTime`` exposes unix seconds, nanosecond fraction, calendar fields, and
  an associated time zone.
