starbytes-disasm
================

``starbytes-disasm`` is the bytecode disassembler binary in the repository.

Current CLI State
-----------------

The current source in ``tools/disasm/disasm.cpp`` exposes the binary but does
not yet implement a usable command-line argument parser or help page. The
present ``main`` function:

* initializes an empty input filename
* tries to open that empty path
* reports ``Failed to read file`` through the diagnostic engine

What This Means Today
---------------------

At the moment, the tool should be treated as an implementation stub rather than
a stable user-facing CLI. The binary exists in ``build/bin/starbytes-disasm``,
but its input path handling is not wired yet.

Documentation Guidance
----------------------

If this tool is expanded in a future change, this page should be updated with:

* a real usage synopsis
* accepted input file types
* output format notes
* any filtering or symbol-annotation options
