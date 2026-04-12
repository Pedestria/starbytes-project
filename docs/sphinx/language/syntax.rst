Syntax Reference
================

Lexical Structure
-----------------

Starbytes source files are UTF-8 compatible and support:

* line comments: ``// ...``
* block comments: ``/* ... */``

Keywords
--------

The documented core keywords include:

``import``, ``scope``, ``class``, ``interface``, ``struct``, ``enum``,
``def``, ``decl``, ``imut``, ``func``, ``lazy``, ``new``, ``return``,
``if``, ``elif``, ``else``, ``while``, ``for``, ``secure``, ``catch``,
``await``, and ``is``.

Literals and Operators
----------------------

Literal forms include:

* strings: ``"text"``
* booleans: ``true`` and ``false``
* integers: ``42``
* floating-point values: ``3.14``
* regex literals: ``/(pattern)/flags``

Supported operator families include arithmetic, assignment, equality,
relational, logical, bitwise, ternary, and the ``is`` runtime type check.

Declarations
------------

Imports use module names and keep imported symbols qualified:

.. code-block:: text

   import IO
   import Net

   decl config = IO.readText("./config.json", "utf-8")
   decl response = Net.httpGet("https://example.com")

Common declaration forms:

.. code-block:: text

   decl name:Type
   decl name = expr
   decl name:Type = expr
   decl imut name = expr

Functions and Type Aliases
--------------------------

Functions may be normal or lazy:

.. code-block:: text

   func add(a:Int, b:Int) Int {
       return a + b
   }

   lazy delayedAdd(a:Int, b:Int) Int {
       return a + b
   }

Aliases use ``def``:

.. code-block:: text

   def UserId = Int
   def Handler = (msg:String) Void
   def Box<T> = Array<T>

Types
-----

Type references support:

* named types such as ``String`` and ``MyType<T>``
* function types such as ``(a:Int,b:Int) Int``
* array suffixes such as ``T[]`` and ``T[][]``
* optional and throwable qualifiers such as ``Int?`` and ``Regex!``

Aggregate Declarations
----------------------

Starbytes currently supports:

* ``class`` for object types with fields, methods, and constructors
* ``interface`` for declaration-only or default-body contracts
* ``struct`` for field-only declarations
* ``enum`` for integer-backed named constants
* ``scope`` for namespaces

Expressions and Statements
--------------------------

Primary expression forms include identifiers, literals, array literals,
dictionary literals, constructor calls, parenthesized expressions, and inline
function expressions.

Control flow includes:

* ``if`` / ``elif`` / ``else``
* ``while``
* condition-form ``for(condition) { ... }``
* ``return``
* ``secure(...) catch { ... }``

Modules and Resolution
----------------------

The language and driver support:

* single-file program roots
* directory module roots
* local and configured search roots
* additional roots from ``.starbmodpath``
* deterministic first-match module resolution
