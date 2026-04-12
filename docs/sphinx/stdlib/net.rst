Net
===

Purpose
-------

ASIO-backed TCP and DNS helpers for lower-level networking workflows.

Types
-----

.. code-block:: text

   def Bytes = Array<Int>
   def StringList = Array<String>

   class TcpSocket {
       func connect(host:String,port:Int) Bool!
       func read(maxBytes:Int) Bytes!
       func write(data:Bytes) Int!
       func writeText(text:String) Int!
       func close() Bool!
       func isOpen() Bool
       func remoteAddress() String
       func remotePort() Int
   }

API Surface
-----------

.. code-block:: text

   func tcpSocket() TcpSocket!
   func resolve(host:String,service:String) StringList!
   func isIPAddress(value:String) Bool

Notes
-----

* ``TcpSocket`` is the stateful client abstraction.
* ``resolve`` returns endpoint text rather than richer socket-address objects.
