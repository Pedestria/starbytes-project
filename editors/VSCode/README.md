# `VSCode`

- starbytes-grammar
- starbytes-vscode

## LSP

The extension starts `starbytes-lsp` for `.starb`/`.stb` files.

Configuration keys:

- `starbytes.lsp.serverPath`: optional explicit path to `starbytes-lsp`.
- `starbytes.lsp.serverArgs`: optional array of extra args.
- `starbytes.lsp.trace.server`: `off|messages|verbose`.

Command:

- `Starbytes: Restart Language Server`

## Build

From this directory:

```bash
npm install
npm run build
npm run package
```

