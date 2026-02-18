import { ExtensionContext, workspace } from "vscode";
import { LanguageClient, LanguageClientOptions, ServerOptions } from "vscode-languageclient";

let langClient: LanguageClient | undefined;

export function activate(context: ExtensionContext): void {
  const serverBinary = context.asAbsolutePath("../../build/bin/starbytes-lsp");
  const serverOptions: ServerOptions = {
    command: serverBinary,
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: "file", language: "starbytes" }],
    synchronize: {
      fileEvents: workspace.createFileSystemWatcher("**/*.starb"),
    },
  };

  langClient = new LanguageClient("starbytesLSP", "Starbytes LSP", serverOptions, clientOptions);
  context.subscriptions.push(langClient.start());
}

export function deactivate(): Thenable<void> | undefined {
  if (!langClient) {
    return undefined;
  }
  return langClient.stop();
}
