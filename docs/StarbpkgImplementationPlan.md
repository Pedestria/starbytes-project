# Starbpkg Full Implementation Plan

Last updated: March 12, 2026

## Purpose

This document proposes how to transform `starbpkg` from its current `.starbmodpath` helper baseline into a fully functional Starbytes project manager and package manager.

The target is a self-hosted tool that can:

- create and manage Starbytes projects
- declare, resolve, fetch, install, and update dependencies
- generate and maintain `.starbmodpath`
- wrap common project workflows (`build`, `run`, `test`, `fmt`, `lint`)
- pack and publish packages
- support local path, git, and registry dependencies
- keep installs reproducible via a lockfile and checksums

## Current State Audit

Current implementation under `/Users/alextopper/Documents/GitHub/starbytes-project/tools/starbpkg`:

- `/Users/alextopper/Documents/GitHub/starbytes-project/tools/starbpkg/starbpkg`
  - bash launcher
  - parses CLI args
  - writes request files to `tools/starbpkg/.request/*`
  - delegates to command-specific Starbytes scripts
- `/Users/alextopper/Documents/GitHub/starbytes-project/tools/starbpkg/init.starb`
  - initializes `.starbpkg/modpaths.txt`
  - writes `.starbmodpath`
- `/Users/alextopper/Documents/GitHub/starbytes-project/tools/starbpkg/sync.starb`
  - regenerates `.starbmodpath`
- `/Users/alextopper/Documents/GitHub/starbytes-project/tools/starbpkg/status.starb`
  - prints current state
- `/Users/alextopper/Documents/GitHub/starbytes-project/tools/starbpkg/main.starb`
  - placeholder help text

What it does today:

- project-local `.starbmodpath` generation
- one-off module-path appends
- simple status output

What it does not do:

- no manifest
- no dependency graph
- no lockfile
- no version solver
- no registry client
- no package cache/store
- no install/update/remove flow
- no build/test/run orchestration
- no packing/publishing
- no workspace support
- no reproducibility/security policy

## Good News: Required Stdlib Surface Already Exists

A full self-hosted implementation is now practical because Starbytes already has the critical stdlib pieces:

- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/CmdLine/CmdLine.starbint`
  - direct argv/flags access
- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/FS/FS.starbint`
  - path and filesystem metadata operations
- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/IO/IO.starbint`
  - file reads/writes and stream IO
- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/JSON/JSON.starbint`
  - lockfile and registry metadata parsing/serialization
- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/HTTP/HTTP.starbint`
  - registry/index fetches and package downloads
- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/Crypto/Crypto.starbint`
  - checksum verification
- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/Archive/Archive.starbint`
  - package archive packing/unpacking baseline
- `/Users/alextopper/Documents/GitHub/starbytes-project/stdlib/Process/Process.starbint`
  - `starbytes`, `starbytes-ling`, and test subprocess invocation

That means the request-file transport and bash wrapper should stop being core architecture. They should become compatibility shims and then be removed.

## Design Principles

1. `starbpkg` should be self-hosted in Starbytes.
2. Source packages first, binary packages later.
3. Reproducibility beats convenience when there is a conflict.
4. Lockfile and checksums are mandatory for install stability.
5. Registry operations must be explicit and diagnosable.
6. `.starbmodpath` should be generated state, never the primary source of truth.
7. The package model should align with interface emission and cache behavior already present in the compiler.

## Recommended Scope Split

### `starbpkg` should own

- project manifests
- dependency resolution
- install store/cache
- `.starbmodpath` generation
- package graph and workspace management
- package packing and publishing
- project command orchestration

### `starbytes` driver should continue to own

- parsing, sema, codegen, runtime execution
- interface emission
- module cache emission and cleanup

### Registry service should own

- package index and package metadata
- tarball/archive hosting
- auth and publish permissions
- compatibility metadata and signatures

## Recommended Project File Model

### Top-level manifest

Use a declarative Starbytes manifest, not JSON.

Recommended file:

- `MODULE.starb`

Design goals:

- keep the manifest in Starbytes syntax so package metadata stays native to the language
- keep the surface minimal and Google-inspired rather than copying npm/cargo naming
- make the manifest declarative and safe to parse without executing arbitrary code
- keep the runtime/build graph readable at a glance

Manifest execution model:

- `starbpkg` should parse `MODULE.starb` as a restricted declarative DSL
- do not allow imports, arbitrary function declarations, subprocess calls, filesystem mutation, or general runtime execution inside manifests
- allow only manifest forms such as `module(...)`, `about(...)`, `use(...)`, `tool(...)`, `lib(...)`, `bin(...)`, and `test(...)`
- treat duplicate or conflicting declarations as manifest validation errors

Recommended shape:

```starbytes
module(
  name: "acme.widgets",
  version: "0.1.0",
  compat: ">=0.12.0 <0.13.0"
)

about(
  summary: "Widget toolkit",
  license: "MIT",
  owners: ["Alex Topper"],
  repo: "https://example.com/acme/widgets",
  publish: true
)

use("json", from: "core", at: "^1.2.0")
use("util", path: "./vendor/util")
tool("testkit", from: "core", at: "^0.4.0")

lib("src/lib.starb")
bin("widgets", "app/main.starb")
test("main", "tests/main.starb")
```

Naming rationale:

- `module`: canonical package identity and compatibility contract
- `about`: human/package metadata
- `use`: runtime/build dependency graph
- `tool`: development-only tools and test-only dependencies
- `lib` / `bin` / `test`: explicit targets instead of script aliases

### Lockfile

Recommended file:

- `MODULE.lock.json`

Lockfile must record:

- resolved dependency graph
- exact versions
- source kind (`path`, `git`, `registry`)
- canonical source URL or commit
- checksum
- compatibility metadata
- install layout / package store key

### Tool state directory

Keep generated manager state under:

- `.starbpkg/`

Recommended contents:

- `.starbpkg/install/`
- `.starbpkg/cache/`
- `.starbpkg/logs/`
- `.starbpkg/registry/`
- `.starbpkg/state.json`
- generated `.starbmodpath`

### Generated `.starbmodpath`

This remains generated from the resolved install graph. It should never be edited directly once a manifest exists.

## Recommended Dependency Source Model

Support these in order:

1. `path`
   - local relative or absolute dependency
   - best for workspace and monorepo development
2. `git`
   - repository URL + commit/tag/branch
   - lock to commit in the lockfile
3. `registry`
   - named registry + semver range
   - resolved to exact version in the lockfile

Do not start with binary package artifacts. Start with source packages only.

Reason:

- Starbytes interface generation and module compilation are still evolving.
- Source installs keep compatibility rules simpler.
- Binary package ABI guarantees should come later.

## Recommended Install Model

Use a content-addressed package store under the project or user cache.

Recommended layout:

- user-global cache root:
  - `~/.starbytes/starbpkg/store/`
- project-local resolved state:
  - `.starbpkg/install/`

Store key should include:

- package name
- exact version
- source fingerprint
- checksum
- Starbytes compatibility fingerprint

Example:

- `~/.starbytes/starbpkg/store/acme.widgets/0.1.0/sha256-.../`

Project-local install entries then point to store paths and are used to generate `.starbmodpath`.

## Registry and Package Format

### Registry index

Start with a simple HTTP + JSON index.

Recommended endpoints:

- `GET /index/packages/<package-name>`
- `GET /artifacts/<package-name>/<version>`
- `POST /publish`

Index metadata should include:

- package name
- version
- checksum
- source artifact URL
- `starbytes_min`
- `starbytes_max`
- dependency list
- license
- publish timestamp
- optional signature metadata

### Package artifact format

For phase 1, package archives can be text-focused source bundles.

Contents should include:

- package source tree
- `MODULE.starb`
- generated interface snapshots if desired
- package metadata manifest

Use `Archive` for the initial artifact format if it is sufficient. If not, extend `Archive` before implementing publish.

## Recommended Command Surface

### Project lifecycle

- `starbpkg new <name>`
- `starbpkg init`
- `starbpkg status`
- `starbpkg doctor`

### Dependency management

- `starbpkg add <package>`
- `starbpkg add --path <path>`
- `starbpkg add --git <url>`
- `starbpkg remove <package>`
- `starbpkg install`
- `starbpkg update`
- `starbpkg sync`
- `starbpkg list`
- `starbpkg why <package>`

### Discovery

- `starbpkg search <query>`
- `starbpkg info <package>`

### Project orchestration

- `starbpkg build`
- `starbpkg run`
- `starbpkg test`
- `starbpkg fmt`
- `starbpkg lint`
- `starbpkg clean`

### Packaging and publishing

- `starbpkg pack`
- `starbpkg publish`
- `starbpkg login`
- `starbpkg logout`

### Workspace and cache

- `starbpkg workspace list`
- `starbpkg workspace sync`
- `starbpkg cache list`
- `starbpkg cache prune`

## Recommended Internal Architecture

Recommended module split under `tools/starbpkg`:

- `Main.starb`
  - CLI entrypoint
- `Cli/`
  - command dispatch
  - help/usage rendering
  - diagnostics formatting
- `Manifest/`
  - manifest schema
  - validation
  - serialization
- `Lockfile/`
  - lockfile schema
  - read/write
- `Resolver/`
  - semver parsing
  - dependency graph construction
  - conflict detection
- `Sources/`
  - path source
  - git source
  - registry source
- `Registry/`
  - index client
  - auth/session handling
- `Installer/`
  - cache/store layout
  - extraction and install state
  - `.starbmodpath` generation
- `Workspace/`
  - multi-package graph support
- `Project/`
  - build/run/test/fmt/lint orchestration
- `Pack/`
  - package archive assembly
  - checksum/signature generation
- `Diagnostics/`
  - user-facing errors and exit code policy

## Immediate Architecture Corrections

These should happen before anything else:

### 1. Remove request-file transport as core behavior

Current request files under `tools/starbpkg/.request/` are a bootstrap hack.

Replace with direct `CmdLine` usage in Starbytes code.

Result:

- bash wrapper becomes optional or thin compatibility shim
- command logic moves into self-hosted Starbytes modules
- tests become cleaner and less stateful

### 2. Introduce a real manifest before adding more commands

Do not expand command count while `.starbmodpath` text files remain the only source of truth.

The manifest must become canonical first.

### 3. Introduce a lockfile before registry support

Do not add semver or remote install flows without a lockfile.

### 4. Make `.starbmodpath` generated from resolved installs

Do not let users hand-maintain `modpaths.txt` long-term once dependency resolution exists.

## Error and Exit Policy

`starbpkg` should adopt a real CLI exit code and error family policy.

Recommended families:

- manifest errors
- dependency resolution errors
- source fetch errors
- checksum/signature errors
- install state errors
- build/test subprocess errors
- publish/auth errors

Recommended behavior:

- human-readable error summary
- structured machine-readable mode later
- actionable next step in each failure path
- no silent partial install state

## Reproducibility and Security Requirements

These are required for a package manager, not optional polish:

- exact lockfile resolution
- checksum verification on downloaded artifacts
- atomic manifest/lockfile writes
- atomic install state writes
- rollback or cleanup on failed install/update
- package name normalization and validation
- version compatibility checks against Starbytes toolchain version
- source provenance recorded in lockfile
- publish-time validation before upload

Later:

- signatures
- trust policy
- authenticated registries

## Build and Workflow Integration

A real project manager must own common workflow commands.

Recommended behavior:

- `build`
  - wraps `starbytes compile`
  - honors manifest target selection
  - materializes correct module path state first
- `run`
  - wraps `starbytes run`
- `test`
  - wraps project test entry or script
- `fmt`
  - wraps `starbytes-ling --pretty-write`
- `lint`
  - wraps `starbytes-ling --lint`
- `clean`
  - removes project-generated package/install/cache state selectively

The manager should not re-implement compilation. It should orchestrate the existing toolchain.

## Workspace Model

A full project manager needs multi-package workspace support.

Recommended workspace file:

- `WORKSPACE.starb`

Recommended shape:

```starbytes
member("packages/core")
member("packages/http")
member("apps/demo")
```

Needed behaviors:

- path dependency auto-linking between workspace members
- single lockfile at workspace root
- topological build/test order
- per-member and whole-workspace commands

## Packaging and Publish Flow

Recommended publish sequence:

1. validate manifest
2. validate version bump policy
3. run package-local checks (`fmt`, `lint`, `build`, `test`) or configured subset
4. render package artifact
5. compute checksum
6. upload artifact + metadata
7. verify registry acceptance

Recommended `pack` output:

- `.starbpkg/out/<name>-<version>.starpkg`
- `.starbpkg/out/<name>-<version>.sha256`

## Phased Delivery Plan

### Phase 0: tool reset and self-hosted CLI

Goal:

- remove request-file dependency
- make `starbpkg` a real Starbytes CLI program

Deliverables:

- `CmdLine`-based CLI parsing
- command dispatch in Starbytes code
- bash wrapper reduced to invoking `starbytes run tools/starbpkg/Main.starb -- ...`
- current commands preserved: `init`, `status`, `sync`, `add-path`

### Phase 1: `MODULE` and lockfile foundation

Goal:

- establish canonical project state

Deliverables:

- `MODULE.starb`
- `MODULE.lock.json`
- declarative manifest parser and validator
- migration from `.starbpkg/modpaths.txt` to manifest-defined dependency/config state
- `.starbmodpath` generation from manifest state
- `new` and real `init`

### Phase 2: dependency model and local sources

Goal:

- make `starbpkg` a real dependency manager without remote complexity yet

Deliverables:

- dependency section schema
- path dependencies
- workspace members
- lockfile generation
- install graph materialization
- commands:
  - `add --path`
  - `remove`
  - `install`
  - `list`
  - `why`

### Phase 3: project workflow orchestration

Goal:

- make `starbpkg` the normal project entrypoint

Deliverables:

- `build`
- `run`
- `test`
- `fmt`
- `lint`
- `clean`
- target selection
- consistent subprocess output handling

### Phase 4: registry client and source packages

Goal:

- enable remote package consumption

Deliverables:

- registry JSON index client over `HTTP`
- semver range parser and resolver
- exact-version lockfile resolution
- global package store/cache
- checksum verification with `Crypto`
- commands:
  - `search`
  - `info`
  - `add <package>`
  - `update`

### Phase 5: git dependencies

Goal:

- support package sources not yet published to a registry

Deliverables:

- git source spec in `MODULE.starb`
- locked commit recording in lockfile
- fetch/update policy
- cache layout for git sources

### Phase 6: pack and publish

Goal:

- complete package lifecycle

Deliverables:

- source bundle format
- `pack`
- `publish`
- package metadata validation
- checksum generation
- auth/session handling if registry requires it

### Phase 7: workspace maturity and cache/policy hardening

Goal:

- make the tool reliable at repository scale

Deliverables:

- workspace root operations
- atomic installs and updates
- failure rollback/cleanup
- `doctor`
- `cache prune`
- warning/error policy hardening
- better diagnostics and logging

## Testing Strategy

### Unit-level

- `MODULE.starb` parser/validator
- lockfile parser/serializer
- semver solver
- dependency graph resolution
- checksum verification
- `.starbmodpath` generation

### Integration-level

- path dependency install
- workspace sync
- build/run/test orchestration
- registry fetch/install using a local mock index
- publish dry-run and package assembly

### Extreme-case regressions

- circular dependencies
- conflicting semver ranges
- duplicate package names from multiple sources
- checksum mismatch
- partial install interruption
- invalid manifest syntax/schema
- invalid package archive
- workspace member missing/renamed

## Recommended Immediate Next Implementation Slice

The correct thin slice is not registry support. It is:

1. rewrite `starbpkg` to consume `CmdLine` directly
2. introduce `MODULE.starb`
3. make `init` and `sync` operate from `MODULE.starb` state
4. preserve `.starbmodpath` generation as generated output

That slice removes the current architecture debt and creates the base required for every later package-manager feature.

## Summary

`starbpkg` should evolve in this order:

1. project helper -> self-hosted CLI
2. self-hosted CLI -> manifest-based project manager
3. project manager -> local dependency manager
4. local dependency manager -> remote registry package manager
5. package manager -> publishable, reproducible ecosystem tool

The current implementation is a valid bootstrap, but it is still only phase-zero scaffolding.

## Naming Direction

If the goal is a minimal and Google-inspired manager surface, prefer these names going forward:

- manifest: `MODULE.starb`
- workspace root: `WORKSPACE.starb`
- lockfile: `MODULE.lock.json`
- dependency field: `use`
- tool/test-only dependency field: `tool`
- target fields: `lib`, `bin`, `test`

Avoid these older ecosystem-shaped names in the long-term design:

- `package`
- `dependencies`
- `devDependencies`
- `scripts`
- `package.json`-style nested metadata as the primary shape
