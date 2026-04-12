CmdLine
=======

Purpose
-------

Runtime command-line argument helpers backed by driver-passed script arguments.

Types and API
-------------

.. code-block:: text

   def StringList = Array<String>

   func rawArgs() StringList
   func argCount() Int
   func executablePath() String
   func scriptPath() String
   func hasFlag(name:String,aliases:StringList) Bool
   func flagValue(name:String,aliases:StringList) String?
   func flagValues(name:String,aliases:StringList) StringList
   func positionals() StringList

Notes
-----

* Arguments forwarded after ``--`` in the driver are visible here.
* ``hasFlag`` and ``flagValue`` are useful when building repo-local tools in
  Starbytes itself, such as ``starbpkg``.
