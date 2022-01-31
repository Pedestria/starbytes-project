#include <llvm/ADT/StringRef.h>
#include <llvm/Support/JSON.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/Optional.h>
#include <starbytes/AST/AST.h>
#include <starbytes/Parser/Parser.h>
#include <chrono>
#include <fstream>

#ifndef STARBYTES_LSP_LSPPROTOCOL_H
#define STARBYTES_LSP_LSPPROTOCOL_H

namespace starbytes::lsp {

#define LSP_METHOD llvm::StringRef 
#define DECL_LSP_METHOD extern LSP_METHOD

DECL_LSP_METHOD Init;
DECL_LSP_METHOD textDocOpen;
DECL_LSP_METHOD Exit;
DECL_LSP_METHOD CancelRequest;

struct MessageInfo {
    llvm::Optional<int> id;
    std::string method;
    llvm::Optional<std::string> progressToken;
};

struct InMessage {
  MessageInfo info;
  bool isNotification = false;
  llvm::json::Value params;
};

enum class OutErrorCode : int {
    NOT_INTIALIZED = -32002
};

struct OutError {
    OutErrorCode code;
    std::string message;
    llvm::Optional<llvm::json::Value> data;
};


struct OutMessage {
  llvm::Optional<llvm::json::Value> result;
  llvm::Optional<llvm::json::Object> error;
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

struct DocumentEntry {
    std::chrono::high_resolution_clock::time_point editTimestamp;
    std::ifstream is;
};

class WorkspaceManager {
    llvm::StringMap<DocumentEntry> openDocuments;
    std::unique_ptr<Parser> parser;
    std::unique_ptr<ASTLocker> locker;
    
    void _tryParse(std::istream & in);
public:
    bool fuzzyMatchInDocumentAtPosition(llvm::StringRef path,SrcLoc &loc,llvm::StringMap<LSPSymbolType> &listOut);
    Semantics::SymbolTable::Entry & getSymbolInfoInDocument(llvm::StringRef path,SrcLoc &refLoc);
    bool openDocument(llvm::StringRef path,SrcLoc &stopPos);
    void closeDocument(llvm::StringRef path);
};


};

#endif // STARBYTES_LSP_LSPPROTOCOL_H
