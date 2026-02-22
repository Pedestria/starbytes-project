## My Proposed Features/Fixes

### FINISHED

- Type casting, down and up, and conversion (Downcasting is a throwable return)
    example usage:
    ```starbytes
    decl a: Float = 4.0
    decl b = Int(a)
    ```
    (For type conversion on custom classes, use conversion functions. `func <type-name> (){}`)
- Maps (Maps are type-strict dictionaries. Dict vs. Map<T,U>)
- Long, Double number support
- Fix symbol resolution inside secure/catch decls (Visible in LSP)
- Fix stability issues for all stdlib modules, with expanded extreme smoke coverage.
- Full type inference coverage for generic collection literals and typed contexts.
- LSP hover shows class/interface inheritance below declarations (superclasses/interfaces).
- Full doxygen spec parsing (`@param`, `@tparam`, `@throws`, `@note`, `@warning`, `@see`, `@since`, `@deprecated`, `@example`, `@code`).
- Builtin types have full doxygen documentation coverage in interface metadata.
- Variable type inference is surfaced in LSP for inferred declarations (builtins/custom classes/invocations), with optional `--infer-64bit-numbers` compiler mode for Long/Double literal inference defaults.

## UNFINISHED
starbpkg bundles all library modules it uses puts them in
