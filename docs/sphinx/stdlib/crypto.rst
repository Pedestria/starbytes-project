Crypto
======

Purpose
-------

Digest, HMAC, PBKDF2, and constant-time helpers that operate on text and hex.

API Surface
-----------

.. code-block:: text

   func md5Hex(text:String) String!
   func sha1Hex(text:String) String!
   func sha256Hex(text:String) String!
   func hmacSha256Hex(key:String,message:String) String!
   func pbkdf2Sha256Hex(password:String,saltHex:String,iterations:Int,keyBytes:Int) String!
   func constantTimeHexEquals(lhsHex:String,rhsHex:String) Bool

Notes
-----

* Digest outputs are lowercase hex strings.
* ``constantTimeHexEquals`` is intended for comparing decoded byte values
  without ordinary short-circuit timing behavior.
