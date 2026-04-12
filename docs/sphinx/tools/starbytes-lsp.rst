starbytes-lsp
=============

``starbytes-lsp`` is the Language Server Protocol implementation for Starbytes.
Its main entrypoint starts a stdio server directly and does not currently expose
a user-facing ``--help`` page or documented CLI flags.

Launch Model
------------

The binary is intended to be started by an editor or client over stdio:

.. code-block:: text

   starbytes-lsp

The current ``main`` function wires:

* stdin as the LSP input stream
* stdout as the LSP output stream
* the in-repo ``Server`` implementation from ``tools/lsp/ServerMain.*``

Supported LSP Surface
---------------------

The server implementation currently includes handlers for:

* initialize, initialized, shutdown, and cancel
* completion and completion resolve
* hover
* definition, declaration, type definition, and implementation
* references
* document symbols and workspace symbols
* prepare rename and rename
* signature help
* document highlight
* document formatting and range formatting
* folding range
* selection range
* code action
* semantic tokens full, range, and delta
* didOpen, didChange, didSave, and didClose notifications

Profiling Knob
--------------

The server source checks one environment variable:

.. code-block:: text

   STARBYTES_LSP_PROFILE=1

When truthy, it enables internal linguistics profiling and related logging in
the server implementation.

Practical Note
--------------

Because the LSP binary is designed for editor integration rather than manual
terminal use, configuration such as server path and args is typically handled by
the VS Code extension or another client wrapper.
