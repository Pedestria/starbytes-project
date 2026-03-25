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
Add a structured error channel to native module callbacks.
Scope module imports under module name (So constants and funcs are under a scope.)
Fix LSP scoped symbol resolution (Hovers should properly resolve symbols in function scopes, classes, etc.)
LSP variable type resolution from module-scoped symbols.
## UNFINISHED

Expand/fix generics: 
    Generic enums/scopes/modules
    Constraints / where clauses / trait bounds
    Variance annotations 

LSP type resolution from the wrapped variable in secure decls.

Remove CodeView's from LSP Hovers.

Implement Qualified type refs

Add compiled-regex caching on Regex objects once the API surface is locked.

Explore the limitation around assigning/casting concrete class instances to interface-typed values. 

Fix contextual expected-type inference

Fix full-file integration (so files execute appropriately)

Move module graphing code and driver to `driver` subsystem in `src`, so that the LSP can use
proper module resolution when searching for modules.

Long bitwise ops in runtime
    width-specific integer semantics beyond current Int
    unsigned rotate helpers like rotl/rotr
    explicit overflow/truncation policy beyond native Int behavior