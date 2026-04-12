Config
======

Purpose
-------

Layered configuration helpers for JSON object parsing, deep merges, environment
overlays, and typed lookups.

Types and API
-------------

.. code-block:: text

   def StringList = Array<String>

   func parseJsonObject(text:String) Dict!
   func merge(base:Dict,overlay:Dict) Dict!
   func fromEnv(prefix:String) Dict
   func requireKeys(config:Dict,keys:StringList) Bool
   func getString(config:Dict,key:String,defaultValue:String) String
   func getInt(config:Dict,key:String,defaultValue:Int) Int

Notes
-----

* ``parseJsonObject`` requires an object root rather than arbitrary JSON.
* ``fromEnv`` strips the prefix from emitted keys.
