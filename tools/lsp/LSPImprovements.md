# LSP Improvements

General TODOS:
- Split the code from one file. (Modular design is easier to read)
- Create a doxygen parser for comments so documentation can be rendered to Markdown so it can express as much detail as possible.
(Add as a seperate class to src/base heirarchy. If codeviews needed to be rendered, use CodeView)
- LSP client can store compiled Symbol Tables locally for quicker reference, unless the source code has changed.
  This will be useful for improved code-completion
- Improve current protocol capabilties.

# Proposed Improvements

# Compiler Warnings/Errors

Send diagnostics to show compiler-driven messages to allow for better code diagnostics.


## Example
```starbytes
    // Warning: Constant `<name>` has no initializer.
    decl imut <name> : <type-name>
```

## Example 2
```starbytes

    func throwableFunction() Int! {
        return 4
    }

    // Warning: Function `throwableFunction` is throwable.
    // Have an option where it can insert the neseccesary fixes. 
    // (Insert a secure/catch statement around the declaration)
    decl <name> = throwableFunction()
```

# Hover

Improve hover capabilities. (Detailed messages, semantic info, and symbol location)

## Hoverable Types (Which tokens are hoverable):

Not Hoverable:
- keywords
- comments
- literals

Hoverable:
- identifiers
- other symbols that reference objects


## Hover Formats (example):

Show the declaration, the surrounding code, comments and  if it has doxgen docuemation, render that it too.
## Example 1 (Regular comment style)
### Constant `<name>` 
```starbytes
// Show parent scope (if it has one) to give people context
scope <scope-name> {
    // Resolve types if they are implied in this context
    decl imut <name> : <type-name> = ...
}
```
Comments

## Example 2 (Out of module class, Regular comment style)
### Class `<name>` 
Imported from `<module-name>`
```starbytes
// Pulled from interface file which shows no class body
class <name> : ...
```
Comments

## Example 2 (Field param or method, Regular comment style)
### Constant `<name>` 
```starbytes
// Show parent class to give people context
class <scope-name> {
    // Resolve types if they are implied in this context
    decl imut <name> : <type-name> = = ...
}
```
Comments

## Example 2(Doxygen styled comments):
### Function `<name>` 
```starbytes

scope <scope-name> {
    // Resolve types if they are implied in this context
    func <name> (param1:type1) <return-type>
}
```
Comments

`param 1` *param documentation*

`returns` *return documentation*






