# Starbytes Project/Toolchain
## New Expressive/Simple/Strongly Typed Interpreted Language.
---

### Contents:
- `starbytes` - The Compiler/Runtime.
- `starbytes-lsp` - The LSP Implementation for Starbytes
- ### Editors (`extension` Directory)
- starbytes-grammar - The Textmate Syntax Grammar for Starbytes.
- starbytes-vscode - The VSCode extension for Starbytes Language. (Provides Grammar ONLY. Executable `starbytes-lspserver` must be externally provided!)

### Dev/Build Instructions (With CMake)
- ### To Setup: 
Run 
```shell
cmake -DCMAKE_BUILD_TYPE:STRING="Debug" -S . -B ./build
``` 
(This will create a build directory where the toolchain libs/exectuables will be outputed to.)
- ### To build:
Run
```shell
cmake --build .
```
(This will build the entire toolchain and output it to build!)
