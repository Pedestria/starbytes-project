starbpkg
========

``starbpkg`` is a Starbytes-native project manager focused on module path setup.
It is launched through a shell wrapper and delegates behavior to
``tools/starbpkg/main.starb``.

Managed Files
-------------

The tool keeps:

* ``<project>/.starbpkg/modpaths.txt``

And generates:

* ``<project>/.starbmodpath``

Usage
-----

.. code-block:: text

   tools/starbpkg/starbpkg [options] <command> [args]

Commands
--------

.. list-table::
   :header-rows: 1

   * - Command
     - Behavior
   * - ``init``
     - Initialize ``.starbpkg`` state and generate ``.starbmodpath``.
   * - ``add-path <dir>``
     - Append a search directory and regenerate ``.starbmodpath``.
   * - ``sync``
     - Regenerate ``.starbmodpath`` from ``modpaths.txt``.
   * - ``status``
     - Print project status and current path configuration.
   * - ``help``
     - Show the command summary.

Options
-------

.. list-table::
   :header-rows: 1

   * - Option
     - Meaning
   * - ``-C, --project <dir>``
     - Target project root. Defaults to the current directory.
   * - ``-B, --starbytes-bin <bin>``
     - Override the Starbytes driver binary used by the shell shim.
   * - ``-V, --verbose``
     - Print launcher debug lines.
   * - ``-h, --help``
     - Show usage.

Defaults and Behavior
---------------------

``init`` seeds ``modpaths.txt`` with:

.. code-block:: text

   ./modules
   ./stdlib

The Starbytes script then mirrors that text into ``.starbmodpath``.

Request Transport
-----------------

The launcher README explains that the wrapper writes request files under
``tools/starbpkg/.request`` and invokes the Starbytes script with forwarded
arguments after ``--``.

Command Rules from Source
-------------------------

The Starbytes implementation enforces:

* ``init``, ``sync``, ``status``, and ``help`` take no extra arguments
* ``add-path`` requires exactly one directory argument
* unknown options and unknown commands return help with an error status
