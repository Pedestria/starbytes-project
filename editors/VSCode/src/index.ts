import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import * as vscode from "vscode";
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  Trace,
} from "vscode-languageclient";

let langClient: LanguageClient | undefined;
let outputChannel: vscode.OutputChannel | undefined;

function expandHome(inputPath: string): string {
  if (inputPath === "~") {
    return os.homedir();
  }
  if (inputPath.startsWith("~/")) {
    return path.join(os.homedir(), inputPath.slice(2));
  }
  return inputPath;
}

function existingPath(candidate: string): string | undefined {
  if (!candidate) {
    return undefined;
  }
  return fs.existsSync(candidate) ? candidate : undefined;
}

function resolveServerCommand(context: vscode.ExtensionContext): string {
  const cfg = vscode.workspace.getConfiguration("starbytes.lsp");
  const configuredPath = cfg.get<string>("serverPath", "").trim();
  if (configuredPath.length > 0) {
    const expanded = expandHome(configuredPath);
    const absolute = path.isAbsolute(expanded) ? expanded : path.resolve(expanded);
    const found = existingPath(absolute);
    return found ?? absolute;
  }

  const binaryName = process.platform === "win32" ? "starbytes-lsp.exe" : "starbytes-lsp";
  const folderCandidates = vscode.workspace.workspaceFolders ?? [];
  for (const folder of folderCandidates) {
    const workspaceRoot = folder.uri.fsPath;
    const candidates = [
      path.join(workspaceRoot, "build", "bin", binaryName),
      path.join(workspaceRoot, "bin", binaryName),
      path.join(workspaceRoot, "tools", "bin", binaryName),
    ];
    for (const candidate of candidates) {
      const found = existingPath(candidate);
      if (found) {
        return found;
      }
    }
  }

  const extensionCandidates = [
    context.asAbsolutePath(path.join("..", "..", "build", "bin", binaryName)),
    context.asAbsolutePath(path.join("..", "..", "bin", binaryName)),
  ];
  for (const candidate of extensionCandidates) {
    const found = existingPath(candidate);
    if (found) {
      return found;
    }
  }

  return binaryName;
}

function mapTraceSetting(raw: string): Trace {
  switch (raw) {
    case "messages":
      return Trace.Messages;
    case "verbose":
      return Trace.Verbose;
    default:
      return Trace.Off;
  }
}

async function startLanguageClient(context: vscode.ExtensionContext): Promise<void> {
  const cfg = vscode.workspace.getConfiguration("starbytes.lsp");
  const command = resolveServerCommand(context);
  const args = cfg.get<string[]>("serverArgs", []);
  const trace = cfg.get<string>("trace.server", "off");
  const maxProblems = cfg.get<number>("maxNumberOfProblems", 100);

  outputChannel = outputChannel ?? vscode.window.createOutputChannel("Starbytes LSP");
  outputChannel.appendLine(`[starbytes] launching LSP: ${command} ${(args ?? []).join(" ")}`);

  const serverOptions: ServerOptions = { command, args };
  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: "file", language: "starbytes" }],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher("**/*.{starb,stb}"),
      configurationSection: "starbytes.lsp",
    },
    outputChannel,
    initializationOptions: {
      maxNumberOfProblems: maxProblems,
    },
  };

  langClient = new LanguageClient("starbytesLSP", "Starbytes LSP", serverOptions, clientOptions);
  langClient.trace = mapTraceSetting(trace);
  await langClient.start();
}

async function stopLanguageClient(): Promise<void> {
  if (langClient) {
    await langClient.stop();
    langClient = undefined;
  }
}

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  context.subscriptions.push(
    vscode.commands.registerCommand("starbytes.restartLanguageServer", async () => {
      await stopLanguageClient();
      await startLanguageClient(context);
      vscode.window.showInformationMessage("Starbytes language server restarted.");
    })
  );

  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration(async (event) => {
      if (!event.affectsConfiguration("starbytes.lsp")) {
        return;
      }
      await stopLanguageClient();
      await startLanguageClient(context);
    })
  );

  await startLanguageClient(context);
}

export async function deactivate(): Promise<void> {
  await stopLanguageClient();
}
