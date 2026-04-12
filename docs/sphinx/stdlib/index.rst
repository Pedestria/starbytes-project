Standard Library
================

The Starbytes stdlib is exposed as native-backed modules declared through
``.starbint`` interface files. Each page below maps to one module.

Conventions
-----------

The signatures in these pages use Starbytes types exactly as declared. A few
quick reminders:

* ``T!`` indicates a throwable result and should usually be handled through
  ``secure(...) catch``.
* ``T?`` indicates an optional result that may be absent.
* ``Dict`` is a dynamic mapping type, while ``Map<K,V>`` is type-strict.

.. toctree::
   :maxdepth: 1

   archive
   cmdline
   compression
   config
   crypto
   env
   fs
   http
   io
   json
   log
   math
   net
   process
   random
   text
   threading
   time
   unicode
