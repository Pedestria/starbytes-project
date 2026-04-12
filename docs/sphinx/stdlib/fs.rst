FS
==

Purpose
-------

Filesystem path, metadata, traversal, and directory helpers. Stream reads and
writes live in ``IO`` to avoid duplication.

Types and API
-------------

.. code-block:: text

   def PathList = Array<String>

   func currentDirectory() String!
   func changeDirectory(path:String) Bool!
   func pathAbsolute(path:String) String!
   func pathNormalize(path:String) String!
   func pathCanonical(path:String) String!
   func pathJoin(parts:PathList) String!
   func pathParent(path:String) String
   func pathFilename(path:String) String
   func pathStem(path:String) String
   func pathExtension(path:String) String
   func pathIsAbsolute(path:String) Bool
   func isFile(path:String) Bool
   func isDirectory(path:String) Bool
   func fileSize(path:String) Int!
   func lastWriteEpochSeconds(path:String) Int!
   func copyPath(src:String,dst:String,recursive:Bool,overwrite:Bool) Bool!
   func removeTree(path:String) Bool!
   func walkPaths(path:String,recursive:Bool,includeDirectories:Bool) PathList!
   func tempDirectory() String!
   func homeDirectory() String!

Notes
-----

* Path helpers split cleanly from file stream helpers in ``IO``.
* ``walkPaths`` is the discovery surface for recursive traversal.
