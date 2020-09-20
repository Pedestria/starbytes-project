import { ExtensionContext,workspace,Disposable, languages} from 'vscode';
import {LanguageClient, ServerOptions, TransportKind, LanguageClientOptions,createClientPipeTransport, generateRandomPipeName, createServerPipeTransport} from "vscode-languageclient"
import * as path from 'path'
let langClient:LanguageClient

export function activate(context:ExtensionContext){
    console.log("Yipee");
    console.log(context.asAbsolutePath("../build/bin/starbytes-lsp.exe"))
    const serverOptions:ServerOptions = {
        command:context.asAbsolutePath("../build/bin/starbytes-lsp.exe"),
    };



    const clientOptions:LanguageClientOptions = {
        documentSelector:[
            {scheme:"file",language:"starbytes"},
            {scheme:"file",language:"starmodule"}
        ],
        synchronize: {
            // Notify the server about file changes to '.clientrc files contained in the workspace
            fileEvents: workspace.createFileSystemWatcher('**/.clientrc')
        },
        
    }

    langClient = new LanguageClient("starbytesLSP","Starbytes LSP",serverOptions,clientOptions);
    context.subscriptions.push(langClient.start());
}

export function deactivate():Promise<void>|undefined{
    if(!langClient){
        return undefined;
    } else {
        return langClient.stop();
    }
}