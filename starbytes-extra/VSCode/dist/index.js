"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode_1 = require("vscode");
const vscode_languageclient_1 = require("vscode-languageclient");
let langClient;
let legend = new vscode_1.SemanticTokensLegend([], []);
class STBSemanticTokenTranslator {
    push(line, char, length, tokenType, tokenModifiers) {
        throw new Error('Method not implemented.');
    }
    build(resultId) {
        throw new Error('Method not implemented.');
    }
}
;
class STBSemanticTokenProvider {
    provideDocumentSemanticTokens(document, token) {
        throw new Error('Method not implemented.');
    }
    provideDocumentSemanticTokensEdits(document, previousResultId, token) {
        throw new Error('Method not implemented.');
    }
}
;
function activate(context) {
    console.log("Yipee");
    console.log(context.asAbsolutePath("../../build/bin/starbytes-lsp"));
    const serverOptions = {
        command: context.asAbsolutePath("../../build/bin/starbytes-lsp")
    };
    const clientOptions = {
        documentSelector: [
            { scheme: "file", language: "starbytes" },
            { scheme: "file", language: "starbytes-project" }
        ],
        synchronize: {
            // Notify the server about file changes to '.clientrc files contained in the workspace
            fileEvents: vscode_1.workspace.createFileSystemWatcher('**/.clientrc')
        },
    };
    langClient = new vscode_languageclient_1.LanguageClient("starbytesLSP", "Starbytes LSP", serverOptions, clientOptions);
    vscode_1.languages.registerHoverProvider({ scheme: "file", language: "starbytes-project" }, {
        provideHover: (doc, pos, token) => {
            console.log("Hovering!");
            let docTokens = doc.getText(doc.getWordRangeAtPosition(pos));
            let result;
            if (/\bmodule\b/g.test(docTokens)) {
                result = "Module Declaration";
            }
            else if (/\bdump\b/g.test(docTokens)) {
                result = "Dump Declaration";
            }
            return new vscode_1.Hover(result);
        }
    });
    context.subscriptions.push(langClient.start());
    //languages.registerDocumentSemanticTokensProvider({scheme:"file",language:"starbytes"},new STBSemanticTokenProvider(),legend);
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
