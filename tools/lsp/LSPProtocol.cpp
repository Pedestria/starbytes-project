#include "LSPProtocol.h"

namespace starbytes::lsp {


LSP_METHOD Init = "initialize";
LSP_METHOD textDocOpen = "textDocument/didOpen";
LSP_METHOD completion = "textDocument/completion";
LSP_METHOD hover = "textDocument/hover";
LSP_METHOD definition = "textDocument/definition";
LSP_METHOD Exit = "shutdown";
LSP_METHOD CancelRequest = "cancelRequest";

void parseTextDocumentPositionParams(llvm::json::Value & param,SrcLoc & loc,std::string & doc){
    auto obj = param.getAsObject();
    auto pos = obj->getObject("position");
    loc.startLine = loc.endLine = (unsigned)pos->getInteger("line").getValue();
    loc.startCol = loc.endCol = (unsigned)pos->getInteger("character").getValue();
    doc = obj->getObject("textDocument")->getString("uri").getValue();
};

void parsePartialResultToken(llvm::json::Value & param,std::string & partialToken){
    auto obj = param.getAsObject();
    auto t = obj->getString("partialResultToken");
    if(t){
        partialToken = t.getValue();
    }
}

void parseWorkDoneToken(llvm::json::Value & param,std::string & workDoneToken){
    auto obj = param.getAsObject();
    auto t = obj->getString("workDoneToken");
    if(t){
        workDoneToken = t.getValue();
    }
}

void writePartialResultToken(llvm::json::Object & param,std::string & partialToken){
    if(!partialToken.empty()){
        param.insert({"partialResultToken",partialToken});
    }
}

void writeWorkDoneToken(llvm::json::Object & param,std::string & workDoneToken){
    if(!workDoneToken.empty()){
        param.insert({"workDoneToken",workDoneToken});
    }
}

}
