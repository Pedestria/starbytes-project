HTTP
====

Purpose
-------

libcurl-backed HTTP client helpers for simple request/response workflows.

Types
-----

.. code-block:: text

   def StringList = Array<String>

   class HttpResponse {
       decl status:Int
       decl body:String
       decl headers:Dict
       decl ok:Bool
   }

API Surface
-----------

.. code-block:: text

   func get(url:String,timeoutMillis:Int,headers:StringList) HttpResponse!
   func post(url:String,body:String,timeoutMillis:Int,headers:StringList) HttpResponse!
   func request(method:String,url:String,body:String,timeoutMillis:Int,headers:StringList) HttpResponse!

Notes
-----

* The ``headers`` parameter is expressed as a string list in the interface.
* ``ok`` is a convenience boolean for 2xx completion.
