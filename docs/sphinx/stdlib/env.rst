Env
===

Purpose
-------

Environment variable access plus platform and process identity helpers.

Types and API
-------------

.. code-block:: text

   def StringList = Array<String>

   func get(key:String) String?
   func set(key:String,value:String) Bool!
   func unset(key:String) Bool!
   func has(key:String) Bool
   func listKeys() StringList
   func osName() String
   func archName() String
   func processId() Int

Notes
-----

* Reads are optional because variables may be absent.
* Mutating functions are throwable because the host environment can reject the
  operation.
