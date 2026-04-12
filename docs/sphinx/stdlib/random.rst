Random
======

Purpose
-------

Deterministic and secure random-value helpers.

API Surface
-----------

.. code-block:: text

   func setSeed(seed:Int) Bool
   func int(min:Int,max:Int) Int!
   func float() Float
   func bytes(count:Int) Array<Int>!
   func secureBytes(count:Int) Array<Int>!
   func secureHex(byteCount:Int) String!
   func uuid4() String!

Notes
-----

* ``setSeed`` affects the deterministic generator path.
* ``secureBytes`` and ``secureHex`` are the cryptographically secure surfaces.
