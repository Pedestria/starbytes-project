Log
===

Purpose
-------

Structured logging helpers with level constants and optional dictionary fields.

Constants
---------

.. code-block:: text

   TRACE = 0
   DEBUG = 1
   INFO = 2
   WARN = 3
   ERROR = 4

API Surface
-----------

.. code-block:: text

   func setMinLevel(level:Int) Bool
   func minLevel() Int
   func log(level:Int,message:String) Bool
   func logWithFields(level:Int,message:String,fields:Dict) Bool
   func trace(message:String) Bool
   func debug(message:String) Bool
   func info(message:String) Bool
   func warn(message:String) Bool
   func error(message:String) Bool

Notes
-----

* ``logWithFields`` is the structured entrypoint.
* The convenience helpers follow the current minimum-level filter.
