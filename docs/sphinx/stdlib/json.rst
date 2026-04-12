JSON
====

Purpose
-------

RapidJSON-backed parsing and serialization for Starbytes runtime values.

API Surface
-----------

.. code-block:: text

   func parse(text:String) Any!
   func isValid(text:String) Bool
   func stringify(value:Any) String!
   func pretty(value:Any,indent:Int) String!

Notes
-----

* ``parse`` may return arrays, dictionaries, scalars, or null-like values.
* ``pretty`` uses a caller-supplied indentation width.
