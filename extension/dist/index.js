"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode_1 = require("vscode");
const vscode_languageclient_1 = require("vscode-languageclient");
let langClient;
function activate(context) {
    console.log("Yipee");
    const serverOptions = {
        command: context.asAbsolutePath("../build/Starbytes/starbytes-lspserver/starbytes-lspserver"),
    };
    const clientOptions = {
        documentSelector: [
            { scheme: "file", language: "starbytes" },
            { scheme: "file", language: "starmodule" }
        ],
        synchronize: {
            // Notify the server about file changes to '.clientrc files contained in the workspace
            fileEvents: vscode_1.workspace.createFileSystemWatcher('**/.clientrc')
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
    else {
        return langClient.stop();
    }
}
exports.deactivate = deactivate;
