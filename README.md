# Starbytes

![LOGO](public/STD%20Logo%20V4.png)

Starbytes is a strongly typed language and toolchain with a bytecode compiler/runtime, native standard library modules, compiler-backed LSP support, and a shared linguistics layer for formatting, linting, and code actions.

Current project version: `0.11.0`

## Repository Contents

This repository contains:
- `starbytes` driver for `run`, `compile`, and `check`
- compiler pipeline: lexer, parser, semantic analysis, bytecode generation, diagnostics
- runtime/interpreter and native-module ABI
- native stdlib modules under `stdlib`
- `starbytes-disasm` bytecode disassembler
- `starbytes-ling` linguist tool for formatting, linting, suggestions, and safe fixes
- `starbytes-lsp` language server
- VS Code extension sources under `editors/VSCode`
- `starbpkg` package-manager/bootstrap helper under `tools/starbpkg`
- benchmark harness under `benchmark`

## Current Language/Tooling Surface

Implemented repo surface includes:
- classes, interfaces, structs, enums, scopes, generics, function types, inline functions, lazy tasks, ternaries, casts, typed arrays/maps, runtime type checks
- scoped module imports: imported module members are referenced as `ModuleName.member`
- native stdlib modules including `IO`, `FS`, `CmdLine`, `Time`, `Threading`, `Unicode`, `Env`, `Process`, `JSON`, `Log`, `Config`, `Math`, `Net`, `HTTP`, `Text`, `Random`, `Crypto`, `Compression`, and `Archive`
- compiler-backed LSP features including diagnostics, hovers, semantic tokens, formatting, range formatting, and code actions
- shared linguistics engine used by both `starbytes-ling` and `starbytes-lsp`

## Prerequisites

Minimum local build requirements:
- `python3`
- `git`
- `cmake` 3.18+
- `ninja`
- C++17 compiler toolchain

Platform notes:
- macOS/Linux: standard `clang` or `gcc` toolchain is sufficient
- Windows: use a Developer Command Prompt or equivalent MSVC-capable environment
- Windows OpenSSL bootstrap requires Perl; `init.py` can auto-download a portable Perl if `AUTOMDEPS` is set

## Bootstrap Dependencies

From repo root:

```bash
python3 init.py
```

`init.py` vendors dependencies into `deps` and bootstraps what the repo expects by default:
- RapidJSON
- PCRE2
- ICU
- Asio
- libcurl
- OpenSSL
- zlib

Notes:
- vendored ICU is preferred by CMake when present and is bootstrapped into `deps/icu/out`
- vendored OpenSSL is bootstrapped into `deps/openssl/out`
- by default ICU tracks the stable maintenance branch configured in `init.py`

Windows example with auto-bootstrapped Perl:

```bat
set AUTOMDEPS=%CD%\deps\automdeps
python init.py
```

## Configure

Debug:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Release:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

WSL cross-compile helper for Windows targets with `clang-cl`:

```bash
./scripts/wsl-cross-win-clangcl.sh
```

## Build

Build the full toolchain:

```bash
cmake --build build
```

Key outputs:
- `build/bin/starbytes`
- `build/bin/starbytes-disasm`
- `build/bin/starbytes-ling`
- `build/bin/starbytes-lsp`
- native stdlib module binaries under `build/stdlib`

If you only need the main driver:

```bash
cmake --build build --target starbytes
```

## Quick Verification

Driver version:

```bash
./build/bin/starbytes --version
```

CTest unit/integration targets:

```bash
ctest --test-dir build --output-on-failure
```

Full repository regression suite:

```bash
./tests/run_full_test_suite.bash
```

## Driver Usage

```bash
# Parse + semantic checks only
./build/bin/starbytes check ./tests/test.starb

# Compile to a .starbmod output
./build/bin/starbytes compile ./tests/test.starb -o ./build/test.starbmod

# Run a script or module directory
./build/bin/starbytes run ./tests/test.starb

# Compile and run with compile profiling
./build/bin/starbytes compile ./tests/test.starb --run --profile-compile
```

Important driver options:
- `--profile-compile`
- `--profile-compile-out <path>`
- `--print-module-path`
- `--clean`
- `--no-native-auto`
- `--infer-64bit-numbers`
- `-n, --native <path>`
- `-L, --native-dir <dir>`
- `-- <script args...>` for runtime argument forwarding

Full help:

```bash
./build/bin/starbytes --help
```

## Other Tools

### `starbytes-ling`

Shared linguistics CLI for formatting and source analysis:

```bash
./build/bin/starbytes-ling --pretty-write ./path/to/file.starb
./build/bin/starbytes-ling --lint ./path/to/file.starb
./build/bin/starbytes-ling --code-actions ./path/to/file.starb
```

### `starbpkg`

Repository-local package/project helper:

```bash
./tools/starbpkg/starbpkg --help
./tools/starbpkg/starbpkg init
./tools/starbpkg/starbpkg status
```

`starbpkg` manages `.starbmodpath` generation from project state.

### `starbytes-lsp` and VS Code extension

The VS Code extension sources live under `editors/VSCode`.

Extension build/package flow:

```bash
cd editors/VSCode
npm install
npm run build
npm run tmbundle
npm run package
```

This produces `editors/VSCode/starbytes-lsp.vsix`.

The extension can point to a local server build using the `starbytes.lsp.serverPath` setting.

## Benchmarks

Track A benchmark harness lives under `benchmark`.

Quick start:

```bash
./benchmark/runners/run_track_a.sh --mode ttfr --runs 20 --warmup 3
```

Parity check against local Python references:

```bash
./benchmark/runners/check_track_a_parity.py
```

## Repository Layout

Top-level directories you will use most often:
- `include` public headers
- `src` compiler, runtime, linguistics, base implementation
- `stdlib` native stdlib modules and builtin interface files
- `tools` CLI tools, LSP, disassembler, package-manager launcher
- `tests` unit tests and full regression suite
- `docs` design docs, plans, protocol matrices, user guide
- `benchmark` cross-language benchmark harness and reports
- `editors/VSCode` VS Code extension sources and packaged artifact

## Reference Docs

Useful project docs:
- `STARBYTES_ROADMAP.md`
- `docs/StarbytesUserGuideISO.md`
- `docs/LSPComplianceMatrix.md`
- `docs/NumericalExecutionImprovementPlan.md`
- `benchmark/README.md`
