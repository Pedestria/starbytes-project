"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const fs = require("fs");
const os = require("os");
const path = require("path");
const vscode = require("vscode");
const vscode_languageclient_1 = require("vscode-languageclient");
let langClient;
let outputChannel;
function expandHome(inputPath) {
    if (inputPath === "~") {
        return os.homedir();
    }
    if (inputPath.startsWith("~/")) {
        return path.join(os.homedir(), inputPath.slice(2));
    }
    return inputPath;
}
function existingPath(candidate) {
    if (!candidate) {
        return undefined;
    }
    return fs.existsSync(candidate) ? candidate : undefined;
}
function uniquePaths(paths) {
    const seen = new Set();
    const out = [];
    for (const candidate of paths) {
        if (!candidate || seen.has(candidate)) {
            continue;
        }
        seen.add(candidate);
        out.push(candidate);
    }
    return out;
}
function expandWorkspaceVariables(inputPath) {
    const primaryWorkspace = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
    if (!primaryWorkspace) {
        return inputPath;
    }
    return inputPath
        .replace(/\$\{workspaceFolder\}/g, primaryWorkspace)
        .replace(/\$\{workspaceRoot\}/g, primaryWorkspace);
}
function isExecutablePath(candidate) {
    if (!existingPath(candidate)) {
        return false;
    }
    if (process.platform === "win32") {
        return true;
    }
    try {
        fs.accessSync(candidate, fs.constants.X_OK);
        return true;
    }
    catch {
        return false;
    }
}
function collectAncestorDirectories(startDir) {
    const ancestors = [];
    let current = path.resolve(startDir);
    while (true) {
        ancestors.push(current);
        const parent = path.dirname(current);
        if (parent === current) {
            break;
        }
        current = parent;
    }
    return ancestors;
}
function candidateBinaryPaths(rootDir, binaryName) {
    return [
        path.join(rootDir, "build-ninja", "bin", binaryName),
        path.join(rootDir, "build", "bin", binaryName),
        path.join(rootDir, "bin", binaryName),
        path.join(rootDir, "tools", "bin", binaryName),
    ];
}
function findBinaryFromRoots(roots, binaryName, searchedPaths) {
    for (const root of roots) {
        for (const ancestor of collectAncestorDirectories(root)) {
            const candidates = candidateBinaryPaths(ancestor, binaryName);
            searchedPaths.push(...candidates);
            for (const candidate of candidates) {
                if (existingPath(candidate) && isExecutablePath(candidate)) {
                    return candidate;
                }
            }
        }
    }
    return undefined;
}
function findBinaryOnPath(binaryName) {
    const rawPath = process.env.PATH ?? "";
    for (const dir of rawPath.split(path.delimiter)) {
        if (!dir) {
            continue;
        }
        const candidate = path.join(dir, binaryName);
        if (existingPath(candidate) && isExecutablePath(candidate)) {
            return candidate;
        }
    }
    return undefined;
}
function resolveConfiguredServerPath(configuredPath) {
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
function resolveServerCommand(context) {
    const cfg = vscode.workspace.getConfiguration("starbytes.lsp");
    const configuredPath = cfg.get("serverPath", "").trim();
    if (configuredPath.length > 0) {
        return resolveConfiguredServerPath(configuredPath);
    }
    const envPath = (process.env.STARBYTES_LSP_PATH ?? "").trim();
    if (envPath.length > 0) {
        return resolveConfiguredServerPath(envPath);
    }
    const binaryName = process.platform === "win32" ? "starbytes-lsp.exe" : "starbytes-lsp";
    const searchedPaths = [];
    const searchRoots = [];
    for (const folder of vscode.workspace.workspaceFolders ?? []) {
        searchRoots.push(folder.uri.fsPath);
    }
    const activeFile = vscode.window.activeTextEditor?.document.uri;
    if (activeFile?.scheme === "file") {
        searchRoots.push(path.dirname(activeFile.fsPath));
    }
    searchRoots.push(context.extensionPath);
    const foundInRoots = findBinaryFromRoots(uniquePaths(searchRoots), binaryName, searchedPaths);
    if (foundInRoots) {
        return foundInRoots;
    }
    const foundOnPath = findBinaryOnPath(binaryName);
    if (foundOnPath) {
        return foundOnPath;
    }
    const searchedPreview = uniquePaths(searchedPaths).slice(0, 12);
    const searchedSuffix = searchedPaths.length > searchedPreview.length ? " ..." : "";
    throw new Error(`Could not find ${binaryName}. Searched: ${searchedPreview.join(", ")}${searchedSuffix}. ` +
        `Set starbytes.lsp.serverPath or STARBYTES_LSP_PATH.`);
}
function resolveServerCwd(command) {
    const primaryWorkspace = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
    if (primaryWorkspace && existingPath(primaryWorkspace)) {
        return primaryWorkspace;
    }
    if (command.includes(path.sep) || command.startsWith(".")) {
        const candidate = path.dirname(command);
        if (existingPath(candidate)) {
            return candidate;
        }
    }
    return undefined;
}
function mapTraceSetting(raw) {
    switch (raw) {
        case "messages":
            return vscode_languageclient_1.Trace.Messages;
        case "verbose":
            return vscode_languageclient_1.Trace.Verbose;
        default:
            return vscode_languageclient_1.Trace.Off;
    }
}
function asErrorMessage(error) {
    if (error instanceof Error) {
        return `${error.name}: ${error.message}`;
    }
    return String(error);
}
async function startLanguageClient(context) {
    const cfg = vscode.workspace.getConfiguration("starbytes.lsp");
    const command = resolveServerCommand(context);
    const args = cfg.get("serverArgs", []);
    const trace = cfg.get("trace.server", "off");
    const maxProblems = cfg.get("maxNumberOfProblems", 100);
    outputChannel = outputChannel ?? vscode.window.createOutputChannel("Starbytes LSP");
    outputChannel.appendLine(`[starbytes] launching LSP: ${command} ${(args ?? []).join(" ")}`);
    const explicitPath = command.includes(path.sep) || command.startsWith(".");
    if (explicitPath && !existingPath(command)) {
        throw new Error(`Language server binary was not found at: ${command}`);
    }
    if (explicitPath && !isExecutablePath(command)) {
        throw new Error(`Language server is not executable: ${command}`);
    }
    const serverOptions = {
        command,
        args,
        transport: vscode_languageclient_1.TransportKind.stdio,
        options: {
            cwd: resolveServerCwd(command),
        },
    };
    const clientOptions = {
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
    langClient = new vscode_languageclient_1.LanguageClient("starbytesLSP", "Starbytes LSP", serverOptions, clientOptions);
    langClient.registerProposedFeatures();
    langClient.trace = mapTraceSetting(trace);
    langClient.start();
    await langClient.onReady();
    outputChannel.appendLine("[starbytes] LSP initialized successfully.");
}
async function stopLanguageClient() {
    if (langClient) {
        await langClient.stop();
        langClient = undefined;
    }
}
async function activate(context) {
    context.subscriptions.push(vscode.commands.registerCommand("starbytes.restartLanguageServer", async () => {
        await stopLanguageClient();
        await startLanguageClient(context);
        vscode.window.showInformationMessage("Starbytes language server restarted.");
    }));
    context.subscriptions.push(vscode.workspace.onDidChangeConfiguration(async (event) => {
        if (!event.affectsConfiguration("starbytes.lsp")) {
            return;
        }
        try {
            await stopLanguageClient();
            await startLanguageClient(context);
        }
        catch (error) {
            const message = asErrorMessage(error);
            outputChannel = outputChannel ?? vscode.window.createOutputChannel("Starbytes LSP");
            outputChannel.appendLine(`[starbytes] failed to restart LSP: ${message}`);
            vscode.window.showErrorMessage(`Starbytes LSP restart failed: ${message}`);
        }
    }));
    try {
        await startLanguageClient(context);
    }
    catch (error) {
        const message = asErrorMessage(error);
        outputChannel = outputChannel ?? vscode.window.createOutputChannel("Starbytes LSP");
        outputChannel.appendLine(`[starbytes] failed to start LSP: ${message}`);
        vscode.window.showErrorMessage(`Starbytes LSP failed to start: ${message}`);
    }
}
exports.activate = activate;
async function deactivate() {
    await stopLanguageClient();
}
exports.deactivate = deactivate;
