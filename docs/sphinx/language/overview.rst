Language Overview
=================

Starbytes is a strongly typed language with a toolchain that centers around
three driver commands:

* ``run`` to compile and execute a script or module root.
* ``compile`` to produce ``.starbmod`` artifacts and related outputs.
* ``check`` to run parser and semantic validation without module output.

Core Language Traits
--------------------

The current implementation and draft guide expose the following core traits:

* UTF-8 source text with line and block comments.
* Strong typing with optional and throwable qualifiers.
* Namespaces via ``scope`` and module-qualified imports.
* Classes, interfaces, structs, enums, and generic aliases.
* Function types, inline functions, and lazy task-returning functions.
* Secure declaration flow for optional and throwable values.
* First-class array, dictionary, map, regex, and task concepts.

Source and Artifact Kinds
-------------------------

The toolchain works with a small set of file and module artifacts:

* ``.starb``: source file
* ``.starbint``: interface file
* ``.starbmod``: compiled module output
* ``.ntstarbmod``: native module binary

The driver also supports module search roots through ``.starbmodpath`` files.

Conformance Direction
---------------------

The draft user guide is written in ISO-style language and treats these
behaviors as part of a coherent language and toolchain contract:

* syntax acceptance
* semantic validation
* runtime execution rules
* deterministic module resolution
* artifact generation
* diagnostics for invalid programs

Documentation Sources
---------------------

This Sphinx section is grounded in these repository sources:

* ``docs/StarbytesUserGuideISO.md``
* ``docs/StarbytesSyntax.md``
* ``docs/StarbytesSemantics.md``
* ``stdlib/builtins.starbint``
