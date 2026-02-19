#include "LSPProtocol.h"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"

namespace starbytes::lsp {


LSP_METHOD Init = "initialize";
LSP_METHOD textDocOpen = "textDocument/didOpen";
LSP_METHOD completion = "textDocument/completion";
LSP_METHOD hover = "textDocument/hover";
LSP_METHOD definition = "textDocument/definition";
LSP_METHOD Exit = "shutdown";
LSP_METHOD CancelRequest = "cancelRequest";

void ThreadedServerContext::resetInputJson(){
    inputJSON.RemoveAllMembers();
    inputJSON.GetAllocator().Clear();
}

void ThreadedServerContext::resetOutputJson(){
    outputJSON.RemoveAllMembers();
    outputJSON.GetAllocator().Clear();
}

MessageIO::MessageIO(std::shared_ptr<ThreadedServerContext> & serverContext):serverContext(serverContext){
    
}

void MessageIO::parseTextDocumentPositionParams(rapidjson::Value & param,Region & loc,std::string & doc){
    auto obj = param.GetObject();
    auto & pos = obj["position"];
    loc.startLine = loc.endLine = (unsigned)pos["line"].GetInt();
    loc.startCol = loc.endCol = (unsigned)pos["character"].GetInt();
    doc = obj["textDocument"]["uri"].GetString();
};

void MessageIO::parsePartialResultToken(rapidjson::Value * param,std::string & partialToken){
    auto obj = param->GetObject();
    auto hasMember = obj.HasMember("partialResultToken");
    if(hasMember){
        partialToken = obj["partialResultToken"].GetString();
    }
}

void MessageIO::parseWorkDoneToken(rapidjson::Value * param,std::string & workDoneToken){
    auto obj = param->GetObject();
    auto hasMember = obj.HasMember("workDoneToken");
    if(hasMember){
        workDoneToken = obj["workDoneToken"].GetString();
    }
}

void MessageIO::writePartialResultToken(rapidjson::Value & param,std::string & partialToken){
    if(!partialToken.empty()){
        param.AddMember("partialResultToken",rapidjson::StringRef(partialToken.c_str()),serverContext->outputJSON.GetAllocator());
    }
}

void MessageIO::writeWorkDoneToken(rapidjson::Value & param,std::string & workDoneToken){
    if(!workDoneToken.empty()){
        param.AddMember("workDoneToken",rapidjson::StringRef(workDoneToken.c_str()),serverContext->outputJSON.GetAllocator());
    }
}

bool WorkspaceManager::documentIsOpen(string_ref path){
    return openDocuments.find(path) != openDocuments.end();
}

Semantics::SymbolTable::Entry &  WorkspaceManager::getSymbolInfoInDocument(string_ref path, Region &refLoc){
    (void)path;
    (void)refLoc;
    static Semantics::SymbolTable::Entry fallbackEntry {};
    return fallbackEntry;
}



}
