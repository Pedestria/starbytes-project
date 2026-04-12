Builtin API
===========

Starbytes ships intrinsic language-level types and helpers outside of the
stdlib module system. The canonical interface surface is declared in
``stdlib/builtins.starbint``.

Global Builtins
---------------

.. code-block:: text

   func print(object:Any) Void

Builtin Type Families
---------------------

Scalar/runtime builtin types:

* ``Void``
* ``Bool``
* ``Int``
* ``Long``
* ``Float``
* ``Double``
* ``String``
* ``Regex``
* ``Any``

Generic/container builtin types:

* ``Array<T>``
* ``Dict``
* ``Map<K,V>``
* ``Task<T>``

String Members
--------------

.. code-block:: text

   decl length:Int
   func isEmpty() Bool
   func at(index:Int) String?
   func slice(start:Int,end:Int) String
   func contains(value:String) Bool
   func startsWith(prefix:String) Bool
   func endsWith(suffix:String) Bool
   func indexOf(value:String) Int
   func lastIndexOf(value:String) Int
   func lower() String
   func upper() String
   func trim() String
   func replace(oldValue:String,newValue:String) String
   func split(separator:String) Array<String>
   func repeat(count:Int) String

Regex Members
-------------

.. code-block:: text

   func match(text:String) Bool!
   func findAll(text:String) Array<String>!
   func replace(text:String,replacement:String) String!

Array Members
-------------

.. code-block:: text

   decl length:Int
   func isEmpty() Bool
   func push(value:T) Int
   func pop() T?
   func at(index:Int) T?
   func set(index:Int,value:T) Bool
   func insert(index:Int,value:T) Bool
   func removeAt(index:Int) T?
   func clear() Bool
   func contains(value:T) Bool
   func indexOf(value:T) Int
   func slice(start:Int,end:Int) Array<T>
   func join(separator:String) String
   func copy() Array<T>
   func reverse() Array<T>

Dict and Map Members
--------------------

``Dict`` exposes dynamic storage keyed by ``DictKey``:

.. code-block:: text

   def DictKey = Int | Long | Float | Double | String

.. code-block:: text

   decl length:Int
   func isEmpty() Bool
   func has(key:DictKey) Bool
   func get(key:DictKey) Any?
   func set(key:DictKey,value:Any) Bool
   func remove(key:DictKey) Any?
   func keys() Array
   func values() Array
   func clear() Bool
   func copy() Dict

``Map<K,V>`` mirrors that shape with type-strict keys and values.

Task
----

``Task<T>`` is the lazy-evaluation task type produced by ``lazy`` functions and
consumed by ``await``. The current builtin declaration surface treats it as a
marker/interface type rather than exposing direct methods.
