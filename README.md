# Starbytes

Starbytes is a strongly typed interpreted language and toolchain.

This repository includes:
- `starbytes` driver (check/compile/run)
- compiler, parser, semantic analysis, runtime
- native stdlib modules (for example `IO`, `Time`, `Threading`, `Unicode`)
- `starbytes-lsp` language server
- VS Code extension sources under `editors/VSCode`

## Prerequisites

- `python3`
- `git`
- `cmake` (3.18+)
- `ninja`
- a C++17 compiler toolchain

## Bootstrap Dependencies

Run the bootstrap script from repo root:

```bash
python3 init.py
```

This fetches third-party dependencies into `deps/` (RapidJSON, PCRE2, ICU).

## Configure (CMake + Ninja)

From repo root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

For release builds:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

## Build

```bash
cmake --build build
```

Main binaries are emitted under `build/bin/`:
- `starbytes`
- `starbytes-lsp`
- `starbytes-disasm`

## Quick Verification

```bash
./build/bin/starbytes --version
ctest --test-dir build --output-on-failure
```

## Basic Driver Usage

```bash
# Parse + semantic checks only
./build/bin/starbytes check ./tests/test.starb

# Compile
./build/bin/starbytes compile ./tests/test.starb -o ./build/test.starbmod

# Compile and run (default command is run)
./build/bin/starbytes run ./tests/test.starb
```

For full CLI options:

```bash
./build/bin/starbytes --help
```
