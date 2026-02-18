<img src="./public/STD Logo.png">


<!-- ##### AppVeyor:

[![Build status](https://ci.appveyor.com/api/projects/status/83o4yjlju2fh93nf?svg=true)](https://ci.appveyor.com/project/Pedestria/starbytes-project) -->




# Starbytes Project/Toolchain
## New Expressive/Simple/Strongly Typed Interpreted Language.
---

##### Travis CI (macOS X):
![](https://travis-ci.com/Pedestria/starbytes-project.svg?token=5K8gmsoNtkbUcnKi4R5v&branch=master)

### NOTE: The starbytes repo is being redone... Come back later..
<!-- ### Main Directory Contents (`starbytes-lang` Directory):
- #### Libraries:
- `AST` - The AST Library
- `Base` - Provides macros and standard functions used throughout the toolchain.
- `ByteCode` - Bytecode parsing and generation.
- `Core` - Central library for starbytes.
- `Semantics` - A collection of libraries for semantic analysis for Starbytes.
- `Gen` - Code generator
- `Parser` - Main parser and Lexer.
- `Project` - Starbytes Project File management.
- `Interpreter` - The Interpreter to run the bytecode.
- `LangDevKit` - Starbytes internal IDE language features provider. (Used by `starbytes-lsp`)
- #### Tools:
- `starbytes-b` - The Compiler.
- `starbytes-r` - The Interpreter.
### Language Server Protocol (`starbytes-lsp` Directory)
- The Language Server Protocol Implementation for Starbytes
### Extra (`starbytes-extra` Directory)
- #### VSCode
- starbytes-grammar - The Textmate Syntax Grammar for Starbytes.
- starbytes-vscode - The VSCode extension for Starbytes Language. (Provides Grammar ONLY. Executable `starbytes-lsp` must be externally provided!) -->

### Dev/Build Instructions (With CMake)

- ### To Setup:
Run

```shell
python3 init.py
```

Run
```shell
cmake -DCMAKE_BUILD_TYPE:STRING="Debug" -S ./starbytes-lang -B ./build
```
(This will create a build directory where the toolchain libs/exectuables will be outputed to.)
- ### To build:
Run
```shell
cmake --build ./build
```
(This will build the entire toolchain and output it to build!)
