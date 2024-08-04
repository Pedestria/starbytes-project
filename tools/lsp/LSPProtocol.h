
#include <rapidjson/fwd.h>
#include <rapidjson/writer.h>
#include <rapidjson/reader.h>
#include <rapidjson/document.h>

#include <starbytes/compiler/AST.h>
#include <starbytes/compiler/Parser.h>
#include <chrono>
#include <fstream>

#ifndef STARBYTES_LSP_LSPPROTOCOL_H
#define STARBYTES_LSP_LSPPROTOCOL_H

namespace starbytes::lsp {
   

#define LSP_METHOD std::string
#define DECL_LSP_METHOD extern LSP_METHOD

DECL_LSP_METHOD Init;
DECL_LSP_METHOD textDocOpen;
DECL_LSP_METHOD completion;
DECL_LSP_METHOD hover;
DECL_LSP_METHOD definition;
DECL_LSP_METHOD Exit;
DECL_LSP_METHOD CancelRequest;




struct MessageInfo {
    std::optional<int> id;
    std::string method;
     std::optional<std::string> progressToken;
};

struct InMessage {
  MessageInfo info;
  bool isNotification = false;
  rapidjson::Value * params;
};

enum class OutErrorCode : int {
    NOT_INTIALIZED = -32002,
    INVALID_PARAMS = -32603
};

struct OutError {
    OutErrorCode code;
    std::string message = "";
    std::optional<rapidjson::Value *> data;
};


struct OutMessage {
  std::optional<rapidjson::Value *> result;
  std::optional<rapidjson::Value *> error;
};


enum class LSPSymbolType : int {
    Variable,
    Class,
    Function,
    Namespace
};


class ASTLocker : public ASTStreamConsumer {
public:
    bool acceptsSymbolTableContext() override {
        return true;
    }
    void consumeDecl(ASTDecl *stmt) override;
    void consumeStmt(ASTStmt *stmt) override;
    void consumeSTableContext(Semantics::STableContext *table) override;
};

/** Context for managing JSON IO in the server*/
struct ThreadedServerContext {

    rapidjson::Document inputJSON;
    
    rapidjson::Document outputJSON;

     void resetInputJson();

    void resetOutputJson();
};

class MessageIO {
    std::shared_ptr<ThreadedServerContext> serverContext;
public:

    void parseTextDocumentPositionParams(rapidjson::Value & param,Region & loc,std::string & doc);

    void parsePartialResultToken(rapidjson::Value * param,std::string & partialToken);

    void parseWorkDoneToken(rapidjson::Value * param,std::string & workDoneToken);

    void writePartialResultToken(rapidjson::Value & param,std::string & partialToken);

    void writeWorkDoneToken(rapidjson::Value & param,std::string & workDoneToken);

    MessageIO(std::shared_ptr<ThreadedServerContext> & serverContext);

};



struct DocumentEntry {
    int version;
    std::ifstream is;
};



class WorkspaceManager {
    string_map<DocumentEntry> openDocuments;
    std::unique_ptr<Parser> parser;
    std::unique_ptr<ASTLocker> locker;

    

    
    void _tryParse(std::istream & in);
public:


    bool fuzzyMatchInDocumentAtPosition(string_ref path,Region &loc,string_map<LSPSymbolType> &listOut);
    Semantics::SymbolTable::Entry & getSymbolInfoInDocument(string_ref path,Region &refLoc);
    bool openDocument(string_ref path,Region &stopPos);
    bool documentIsOpen(string_ref path);
    void closeDocument(string_ref path);
};




};

#endif // STARBYTES_LSP_LSPPROTOCOL_H
