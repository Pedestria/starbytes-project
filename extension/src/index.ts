import { ExtensionContext } from 'vscode';
import {LanguageClient, ServerOptions, TransportKind, LanguageClientOptions} from "vscode-languageclient"

let langClient:LanguageClient

export function activate(context:ExtensionContext){
    console.log("Yipee");
    // const serverOptions:ServerOptions = {
    //     command:"starbytes-serverlsp",
    //     args:[]
    // };
    // const clientOptions:LanguageClientOptions = {
    //     documentSelector:[
    //         {scheme:"file",language:"starbytes"}
    //     ]
    // }

}

export function deactivate():Promise<void>|undefined{
    if(!langClient){
        return undefined;
    } else {
        langClient.stop();
    }
}