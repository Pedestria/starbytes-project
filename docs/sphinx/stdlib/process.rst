Process
=======

Purpose
-------

Synchronous subprocess execution with captured output and exit status.

Types
-----

.. code-block:: text

   def StringList = Array<String>

   class ProcessResult {
       decl exitCode:Int
       decl output:String
       decl success:Bool
   }

API Surface
-----------

.. code-block:: text

   func run(command:String) ProcessResult!
   func runArgs(program:String,args:StringList) ProcessResult!
   func shellQuote(value:String) String

Notes
-----

* ``run`` executes a raw shell command string.
* ``runArgs`` is the safer program-plus-argv form when argument boundaries
  matter.
