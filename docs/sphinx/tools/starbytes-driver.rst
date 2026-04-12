starbytes Driver
================

The main ``starbytes`` binary is the primary compiler/runtime driver. Its help
surface is stable enough to document directly from the built binary.

Usage
-----

.. code-block:: text

   starbytes [command] <script.starb|module_dir> [options] [-- <script args...>]
   starbytes --help
   starbytes --version

Commands
--------

.. list-table::
   :header-rows: 1

   * - Command
     - Behavior
   * - ``run``
     - Compile and execute a script or module folder. This is the default.
   * - ``compile``
     - Compile a script or module folder into a ``.starbmod`` module.
   * - ``check``
     - Parse and run semantic checks without writing module output.
   * - ``help``
     - Print the help page.

Options
-------

.. list-table::
   :header-rows: 1

   * - Option
     - Meaning
   * - ``-h, --help``
     - Show help.
   * - ``-V, --version``
     - Show the driver version.
   * - ``-m, --modulename <name>``
     - Override the inferred module name.
   * - ``-o, --output <file>``
     - Write compiled output to an explicit module path.
   * - ``-d, --out-dir <dir>``
     - Set the output directory for compiled artifacts.
   * - ``--run``
     - Execute after compile.
   * - ``--no-run``
     - Skip execution after compile.
   * - ``--clean``
     - Remove generated output artifacts and compile cache on success.
   * - ``--print-module-path``
     - Print the resolved output module path.
   * - ``--profile-compile``
     - Print structured compile phase timings.
   * - ``--profile-compile-out <path>``
     - Write compile profile output to a file.
   * - ``--profile-runtime``
     - Print runtime profile summary after execution.
   * - ``--profile-runtime-out <path>``
     - Write detailed runtime profile output to a file.
   * - ``--bytecode-version <ver>``
     - Emit ``v1`` or ``v2`` bytecode.
   * - ``--runtime-mode <mode>``
     - Select runtime path: ``auto``, ``v1``, or ``v2``.
   * - ``--no-diagnostics``
     - Suppress buffered runtime diagnostics.
   * - ``-n, --native <path>``
     - Preload a native module binary. Repeatable.
   * - ``-L, --native-dir <dir>``
     - Add a search directory for automatic native module resolution.
   * - ``-j, --jobs <count>``
     - Set parallel module build jobs.
   * - ``--no-native-auto``
     - Disable automatic native module resolution from imports.
   * - ``--infer-64bit-numbers``
     - Infer integer literals as ``Long`` and floating literals as ``Double``.
   * - ``-- <args...>``
     - Forward remaining arguments to the runtime for the ``CmdLine`` module.

Examples
--------

.. code-block:: text

   starbytes hello.starb
   starbytes run ./app
   starbytes run hello.starb -m HelloApp
   starbytes compile hello.starb -o build/hello.starbmod
   starbytes check ./libmodule
   starbytes run app.starb -n ./build/stdlib/libIO.ntstarbmod

Operational Notes
-----------------

The driver is also where module-resolution behavior, compile/runtime profiling,
native module loading, and artifact naming come together. If you need the most
authoritative CLI contract, this is the first binary to consult.
