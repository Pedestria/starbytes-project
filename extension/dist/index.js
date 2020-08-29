"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
let langClient;
function activate(context) {
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
