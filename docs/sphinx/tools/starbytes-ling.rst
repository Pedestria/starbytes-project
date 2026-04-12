starbytes-ling
==============

``starbytes-ling`` is the shared linguistics CLI for formatting, linting,
suggestions, and safe code actions across Starbytes source trees.

Usage
-----

.. code-block:: text

   starbytes-ling --pretty-write <source.starb|dir>
   starbytes-ling --lint <source.starb|dir>
   starbytes-ling --suggest <source.starb|dir>
   starbytes-ling --code-actions <source.starb|dir>
   starbytes-ling --apply-safe-fixes [--dry-run] <source.starb|dir>
   starbytes-ling --help
   starbytes-ling --version

Options
-------

.. list-table::
   :header-rows: 1

   * - Option
     - Meaning
   * - ``-h, --help``
     - Show help.
   * - ``-V, --version``
     - Show tool version.
   * - ``--pretty-write``
     - Emit formatted source to stdout.
   * - ``--lint``
     - Run the lint engine and print findings.
   * - ``--syntax-only``
     - With ``--lint``, print compiler syntax diagnostics only.
   * - ``--semantic-only``
     - With ``--lint``, print semantic lint findings only.
   * - ``--suggest``
     - Run the suggestion engine and print suggestions.
   * - ``--code-actions``
     - Build code actions from lint and suggestion results.
   * - ``--apply-safe-fixes``
     - Apply safe actions in place.
   * - ``--dry-run``
     - Preview safe-fix output without writing files.
   * - ``--modpath-aware``
     - Load additional roots from ``.starbmodpath``.
   * - ``--include <glob>``
     - Add an include glob. Repeatable.
   * - ``--exclude <glob>``
     - Add an exclude glob. Repeatable.
   * - ``--max-files <n>``
     - Cap discovered file count. ``0`` means unlimited.

Typical Workflows
-----------------

.. code-block:: text

   starbytes-ling --pretty-write ./tests/test.starb
   starbytes-ling --lint ./src
   starbytes-ling --suggest ./src
   starbytes-ling --code-actions ./src
   starbytes-ling --apply-safe-fixes --dry-run ./src

Behavior Notes
--------------

The source-level argument parser enforces a few important constraints:

* exactly one operation must be selected
* ``--syntax-only`` and ``--semantic-only`` require ``--lint``
* ``--dry-run`` requires ``--apply-safe-fixes``
* exactly one positional input path is required
