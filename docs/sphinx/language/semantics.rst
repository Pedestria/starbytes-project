Semantics Reference
===================

Evaluation Model
----------------

The current Starbytes implementation compiles to runtime bytecode and requires
semantic analysis before execution. Parser and semantic passes establish the
shape that later code generation and runtime execution depend on.

Type System Rules
-----------------

Important matching rules from the current semantics reference:

* ``Any`` is a compatibility bridge in either direction.
* Optional values (``T?``) do not flow into non-optional targets without
  secure handling.
* Throwable values (``T!``) do not flow into non-throwable targets without
  secure handling.
* Generic arity and each type argument must match recursively.
* ``Task`` is validated as a generic type carrying exactly one type argument.

Secure Declarations
-------------------

Optional and throwable initializers must be handled through secure declaration
flow when assigned into ordinary declarations:

.. code-block:: text

   secure(decl value = maybe()) catch (err:String) {
       print(err)
   }

The catch block executes when the guarded initializer yields absence or failure.

Scope and Name Resolution
-------------------------

The compiler models several scope kinds:

* namespace scope
* class scope
* function scope
* block scopes for conditionals, loops, and secure/catch regions

Imported members are not injected unqualified into file scope. They are
accessed through their module scope such as ``IO.readText(...)``.

Class and Interface Semantics
-----------------------------

Current rules include:

* a class may have at most one superclass
* remaining inheritance entries must be interfaces
* duplicate method names in the same class are rejected
* constructor overload keys are based on arity
* interface fields and methods must be satisfied by implementing classes

Attribute Semantics
-------------------

Validated system attributes include:

* ``@readonly``
* ``@private``
* ``@protected``
* ``@deprecated(...)``
* ``@native(...)``

The semantics layer validates placement and argument shape for these
attributes. ``@protected`` is currently enforced in semantic analysis for the
supported member types.

Runtime and Await
-----------------

Lazy functions evaluate to ``Task<T>`` at the call site. ``await`` requires a
task type and yields the resolved ``T``. The draft guide describes a
single-threaded microtask-style model rather than requiring extra worker
threads.

Collections and Indexing
------------------------

The current semantics document highlights these behaviors:

* ``Array[index]`` requires an ``Int`` index.
* ``String[index]`` uses Unicode scalar indexing and returns ``String?``.
* ``Dict[key]`` accepts string or numeric keys under the current builtin rules.
* ``Map<K,V>`` enforces key and value types at the type-system layer.
