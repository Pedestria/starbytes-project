import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import * as vscode from "vscode";
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind,
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

function expandWorkspaceVariables(inputPath: string): string {
  const primaryWorkspace = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
  if (!primaryWorkspace) {
    return inputPath;
  }
  return inputPath
    .replace(/\$\{workspaceFolder\}/g, primaryWorkspace)
    .replace(/\$\{workspaceRoot\}/g, primaryWorkspace);
}

function isExecutablePath(candidate: string): boolean {
  if (!existingPath(candidate)) {
    return false;
  }
  if (process.platform === "win32") {
    return true;
  }
  try {
    fs.accessSync(candidate, fs.constants.X_OK);
    return true;
  } catch {
    return false;
  }
}

function resolveConfiguredServerPath(configuredPath: string): string {
  const expanded = expandWorkspaceVariables(expandHome(configuredPath));
  if (path.isAbsolute(expanded)) {
    return expanded;
  }

  for (const folder of vscode.workspace.workspaceFolders ?? []) {
    const candidate = path.join(folder.uri.fsPath, expanded);
    if (existingPath(candidate)) {
      return candidate;
    }
  }

  return path.resolve(expanded);
}

function resolveServerCommand(context: vscode.ExtensionContext): string {
  const cfg = vscode.workspace.getConfiguration("starbytes.lsp");
  const configuredPath = cfg.get<string>("serverPath", "").trim();
  if (configuredPath.length > 0) {
    return resolveConfiguredServerPath(configuredPath);
  }

  const binaryName = process.platform === "win32" ? "starbytes-lsp.exe" : "starbytes-lsp";
  const folderCandidates = vscode.workspace.workspaceFolders ?? [];
  for (const folder of folderCandidates) {
    const workspaceRoot = folder.uri.fsPath;
    const candidates = [
      path.join(workspaceRoot, "build-ninja", "bin", binaryName),
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

function asErrorMessage(error: unknown): string {
  if (error instanceof Error) {
    return `${error.name}: ${error.message}`;
  }
  return String(error);
}

async function startLanguageClient(context: vscode.ExtensionContext): Promise<void> {
  const cfg = vscode.workspace.getConfiguration("starbytes.lsp");
  const command = resolveServerCommand(context);
  const args = cfg.get<string[]>("serverArgs", []);
  const trace = cfg.get<string>("trace.server", "off");
  const maxProblems = cfg.get<number>("maxNumberOfProblems", 100);

  outputChannel = outputChannel ?? vscode.window.createOutputChannel("Starbytes LSP");
  outputChannel.appendLine(`[starbytes] launching LSP: ${command} ${(args ?? []).join(" ")}`);

  const explicitPath = command.includes(path.sep) || command.startsWith(".");
  if (explicitPath && !existingPath(command)) {
    throw new Error(`Language server binary was not found at: ${command}`);
  }
  if (explicitPath && !isExecutablePath(command)) {
    throw new Error(`Language server is not executable: ${command}`);
  }

  const serverOptions: ServerOptions = {
    command,
    args,
    transport: TransportKind.stdio,
  };
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
  langClient.start();
  await langClient.onReady();
  outputChannel.appendLine("[starbytes] LSP initialized successfully.");
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
      try {
        await stopLanguageClient();
        await startLanguageClient(context);
      } catch (error) {
        const message = asErrorMessage(error);
        outputChannel = outputChannel ?? vscode.window.createOutputChannel("Starbytes LSP");
        outputChannel.appendLine(`[starbytes] failed to restart LSP: ${message}`);
        vscode.window.showErrorMessage(`Starbytes LSP restart failed: ${message}`);
      }
    })
  );

  try {
    await startLanguageClient(context);
  } catch (error) {
    const message = asErrorMessage(error);
    outputChannel = outputChannel ?? vscode.window.createOutputChannel("Starbytes LSP");
    outputChannel.appendLine(`[starbytes] failed to start LSP: ${message}`);
    vscode.window.showErrorMessage(`Starbytes LSP failed to start: ${message}`);
  }
}

export async function deactivate(): Promise<void> {
  await stopLanguageClient();
}
