"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode_1 = require("vscode");
const vscode_languageclient_1 = require("vscode-languageclient");
let langClient;
function activate(context) {
    const serverBinary = context.asAbsolutePath("../../build/bin/starbytes-lsp");
    const serverOptions = {
        command: serverBinary,
    };
    const clientOptions = {
        documentSelector: [{ scheme: "file", language: "starbytes" }],
        synchronize: {
            fileEvents: vscode_1.workspace.createFileSystemWatcher("**/*.starb"),
        },
    };
    langClient = new vscode_languageclient_1.LanguageClient("starbytesLSP", "Starbytes LSP", serverOptions, clientOptions);
    context.subscriptions.push(langClient.start());
}
exports.activate = activate;
function deactivate() {
    if (!langClient) {
        return undefined;
    }
    return langClient.stop();
}
exports.deactivate = deactivate;
