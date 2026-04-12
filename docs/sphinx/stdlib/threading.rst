Threading
=========

Purpose
-------

Concurrency primitives for host-level coordination. The module explicitly notes
that background execution of Starbytes closures is not implemented yet.

Constants
---------

.. code-block:: text

   WAIT_FOREVER = -1

Core Types
----------

.. code-block:: text

   class Mutex
   class Condition
   class Semaphore
   class Event

Factory Functions
-----------------

.. code-block:: text

   func mutexCreate() Mutex!
   func conditionCreate() Condition!
   func semaphoreCreate(initial:Int,max:Int) Semaphore!
   func eventCreate(initial:Bool,manualReset:Bool) Event!
   func currentThreadId() String
   func hardwareConcurrency() Int
   func yieldNow() Bool!
   func sleepMillis(ms:Int) Bool!

Representative Members
----------------------

* ``Mutex``: ``lock``, ``tryLock``, ``unlock``
* ``Condition``: ``wait``, ``notifyOne``, ``notifyAll``
* ``Semaphore``: ``acquire``, ``release``, ``currentCount``
* ``Event``: ``wait``, ``set``, ``reset``, ``isSet``

Notes
-----

Timeout-taking waits accept milliseconds or ``WAIT_FOREVER``.
