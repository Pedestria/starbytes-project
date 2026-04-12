IO
==

Purpose
-------

Text and binary file streams plus convenience filesystem helpers.

Constants
---------

.. code-block:: text

   SEEK_START = 0
   SEEK_CURRENT = 1
   SEEK_END = 2

Core Types
----------

.. code-block:: text

   def Bytes = Array<Int>

   interface Closable
   interface ReadableText
   interface WritableText
   interface ReadableBytes
   interface WritableBytes
   interface Seekable
   interface IOBase
   interface TextIOBase
   interface BufferedIOBase

   class TextFile : TextIOBase
   class BinaryFile : BufferedIOBase
   class Stream : TextFile

Construction and Helpers
------------------------

.. code-block:: text

   func openText(path:String,mode:String,encoding:String,newline:String,buffering:Int) TextFile!
   func openBinary(path:String,mode:String,buffering:Int) BinaryFile!
   func openFile(file:String,mode:String) Stream!
   func exists(path:String) Bool
   func remove(path:String) Bool!
   func rename(src:String,dst:String) Bool!
   func createDirectory(path:String,recursive:Bool) Bool!
   func listDirectory(path:String) Array<String>!
   func readText(path:String,encoding:String) String!
   func writeText(path:String,text:String,encoding:String) Bool!

Stream Capabilities
-------------------

``TextFile`` supports close, read, line reads, write, seek, truncate, flush,
readability checks, encoding introspection, and newline reporting.

``BinaryFile`` supports close, byte reads, byte writes, seek, truncate, flush,
and capability queries.
