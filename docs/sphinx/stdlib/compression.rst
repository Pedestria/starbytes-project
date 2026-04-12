Compression
===========

Purpose
-------

zlib-backed compression helpers with hex payload surfaces.

API Surface
-----------

.. code-block:: text

   func deflateHex(inputHex:String,level:Int) String!
   func inflateHex(compressedHex:String) String!
   func gzipTextHex(text:String,level:Int) String!
   func gunzipText(compressedHex:String) String!
   func crc32Hex(text:String) String!

Notes
-----

* The module mixes raw hex transforms and UTF-8 text convenience helpers.
* Compression level parameters follow the native backend contract.
