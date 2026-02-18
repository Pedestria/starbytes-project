# Starbytes Docs Index

This file now serves as an index to the canonical Starbytes language docs.

## Canonical References

- Syntax reference: `docs/StarbytesSyntax.md`
- Compiler and driver workflow: `docs/StarbytesCompilerWorkflow.md`
- Static and runtime semantics: `docs/StarbytesSemantics.md`

## Why this file changed

`docs/StarbytesSyntaxDesign.md` previously mixed implemented behavior and planned behavior in one place.

To keep specification quality high, the detailed documentation was split by concern:

1. syntax and grammar
2. compiler/driver workflow
3. semantic rules

Use the three files above as the source of truth for current implementation.
