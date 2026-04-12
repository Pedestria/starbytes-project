Text
====

Purpose
-------

Advanced text transforms layered on builtin regex and Unicode semantics.

Types and API
-------------

.. code-block:: text

   def StringList = Array<String>

   func caseFold(text:String,locale:String) String!
   func equalsFold(lhs:String,rhs:String,locale:String) Bool
   func words(text:String,locale:String) StringList!
   func lines(text:String,locale:String) StringList!

Notes
-----

* ``Text`` is a higher-level convenience layer.
* The lower-level ICU-backed segmentation and locale objects live in
  :doc:`unicode`.
