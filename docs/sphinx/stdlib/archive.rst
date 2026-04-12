Archive
=======

Purpose
-------

In-memory archive packing and unpacking with optional compression, exposed
through hex payloads.

API Surface
-----------

.. code-block:: text

   func packTextMapHex(entries:Dict,compress:Bool) String!
   func unpackTextMapHex(archiveHex:String) Dict!
   func listEntries(archiveHex:String) Array<String>!
   func isValid(archiveHex:String) Bool

Notes
-----

* The surface is text-oriented: packed entries are ``Dict<String,String>`` in
  practice.
* The throwable APIs should be wrapped when decoding untrusted payloads.
