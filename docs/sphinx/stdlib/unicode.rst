Unicode
=======

Purpose
-------

ICU-backed normalization, casing, segmentation, collation, and scalar metadata
APIs.

Constants
---------

.. code-block:: text

   NFC = 0
   NFD = 1
   NFKC = 2
   NFKD = 3

   WORD = 0
   SENTENCE = 1
   LINE = 2
   GRAPHEME = 3

Core Types
----------

.. code-block:: text

   class Locale
   class Collator
   class BreakIterator
   class UnicodeScalarInfo

Representative API
------------------

.. code-block:: text

   func normalize(text:String,form:Int) String!
   func isNormalized(text:String,form:Int) Bool!
   func caseFold(text:String) String!
   func localeCurrent() Locale!
   func localeRoot() Locale!
   func localeFrom(identifier:String) Locale!
   func collatorCreate(locale:Locale) Collator!
   func breakIteratorCreate(kind:Int,locale:Locale,text:String) BreakIterator!
   func toUpper(text:String,locale:Locale) String!
   func toLower(text:String,locale:Locale) String!
   func toTitle(text:String,locale:Locale) String!
   func graphemes(text:String,locale:Locale) Array<String>!
   func words(text:String,locale:Locale) Array<String>!
   func sentences(text:String,locale:Locale) Array<String>!
   func lines(text:String,locale:Locale) Array<String>!
   func codepoints(text:String) Array<Int>!
   func fromCodepoints(values:Array<Int>) String!
   func scalarInfo(codepoint:Int) UnicodeScalarInfo!
   func containsFolded(haystack:String,needle:String,locale:Locale) Bool!
   func startsWithFolded(text:String,prefix:String,locale:Locale) Bool!
   func endsWithFolded(text:String,suffix:String,locale:Locale) Bool!
   func displayWidth(text:String,locale:Locale) Int!

Notes
-----

* ``Collator`` and ``BreakIterator`` are explicit objects rather than hidden
  globals.
* This module is the low-level locale-aware text surface behind several higher
  level helpers elsewhere in the stdlib.
