# starbpkg

`starbpkg` is a Starbytes-native project manager focused on module path setup.

It keeps a managed source file:

- `<project>/.starbpkg/modpaths.txt`

And generates:

- `<project>/.starbmodpath`

## Commands

- `init`
  - creates `<project>/.starbpkg/modpaths.txt` with defaults (`./modules`, `./stdlib`)
  - generates `<project>/.starbmodpath`
- `add-path <dir>`
  - appends `<dir>` to `modpaths.txt`
  - regenerates `.starbmodpath`
- `sync`
  - regenerates `.starbmodpath` from `modpaths.txt`
- `status`
  - prints current project paths and current file contents
- `help`
  - prints command summary

## Launcher usage

```bash
tools/starbpkg/starbpkg [options] <command> [args]
```

Options:

- `-C, --project <dir>`: target project root (default current directory)
- `-B, --starbytes-bin <bin>`: Starbytes driver binary (default `build/bin/starbytes`)
- `-V, --verbose`: print launcher debug lines
- `-h, --help`: show usage

## Request transport

Because Starbytes scripts do not receive CLI argv directly, the launcher writes
request files to:

- `tools/starbpkg/.request/project.txt`
- `tools/starbpkg/.request/arg1.txt`

Then executes one command-specific Starbytes script:

- `tools/starbpkg/init.starb`
- `tools/starbpkg/sync.starb`
- `tools/starbpkg/status.starb`
