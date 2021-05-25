import { ExtensionContext,workspace,Disposable, languages, SemanticTokensBuilder, Range, SemanticTokens, SemanticTokensLegend, DocumentSemanticTokensProvider, CancellationToken, Event, ProviderResult, SemanticTokensEdits, TextDocument, Hover} from 'vscode';
import {LanguageClient, ServerOptions, TransportKind, LanguageClientOptions,createClientPipeTransport, generateRandomPipeName, createServerPipeTransport} from "vscode-languageclient"
import * as path from 'path'
let langClient:LanguageClient

let legend = new SemanticTokensLegend([],[]);


class STBSemanticTokenTranslator implements SemanticTokensBuilder {
    push(line: number, char: number, length: number, tokenType: number, tokenModifiers?: number): void;
    push(range: Range, tokenType: string, tokenModifiers?: string[]): void;
    push(line: any, char: any, length?: any, tokenType?: any, tokenModifiers?: any) {
        throw new Error('Method not implemented.');
    }
    build(resultId?: string): SemanticTokens {
        throw new Error('Method not implemented.');
    }

};

class STBSemanticTokenProvider implements DocumentSemanticTokensProvider {
    onDidChangeSemanticTokens?: Event<void>;
    provideDocumentSemanticTokens(document: TextDocument, token: CancellationToken): ProviderResult<SemanticTokens> {
        throw new Error('Method not implemented.');
    }
    provideDocumentSemanticTokensEdits?(document: TextDocument, previousResultId: string, token: CancellationToken): ProviderResult<SemanticTokens | SemanticTokensEdits> {
        throw new Error('Method not implemented.');
    }
    
};

export function activate(context:ExtensionContext){
    console.log("Yipee");
    console.log(context.asAbsolutePath("../../build/bin/starbytes-lsp"))
    const serverOptions:ServerOptions = {
        command:context.asAbsolutePath("../../build/bin/starbytes-lsp"),
        options:{detached:true}
    };



    const clientOptions:LanguageClientOptions = {
        documentSelector:[
            {scheme:"file",language:"starbytes"},
            {scheme:"file",language:"starbytes-project"}
        ],
        synchronize: {
            // Notify the server about file changes to '.clientrc files contained in the workspace
            fileEvents: workspace.createFileSystemWatcher('**/.clientrc')
        },
        
    }

    langClient = new LanguageClient("starbytesLSP","Starbytes LSP",serverOptions,clientOptions);
    languages.registerHoverProvider({scheme:"file",language:"starbytes-project"},{
        provideHover: (doc,pos,token) => {
            let docTokens = doc.getText(doc.getWordRangeAtPosition(pos));
            console.log(docTokens);
            let result:string;
            if(/\bmodule\b/g.test(docTokens)){
                result = "Module Declaration";
            } else if(/\bdump\b/g.test(docTokens)){
                result = "Dump Declaration"
            }
            return new Hover(result);
        }
    })
    context.subscriptions.push(langClient.start());
    //languages.registerDocumentSemanticTokensProvider({scheme:"file",language:"starbytes"},new STBSemanticTokenProvider(),legend);
    
}

export function deactivate():Promise<void>|undefined{
    if(!langClient){
        return undefined;
    } else {
        return langClient.stop();
    }
}