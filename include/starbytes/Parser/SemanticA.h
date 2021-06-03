#include "starbytes/Syntax/SyntaxA.h"
#include <fstream>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/ArrayRef.h>
#include "SymTable.h"

#ifndef STARBYTES_PARSER_SEMANTICA_H
#define STARBYTES_PARSER_SEMANTICA_H

namespace starbytes {

    struct ASTScope;

    struct SemanticADiagnostic final : public Diagnostic {
        typedef enum : int {
          Error,
          Warning,
          Suggestion
        } Ty;
        Ty type;
        ASTStmt *stmt;
        std::string message;
        bool isError() override{
            return type == Error;
        };
        void format(llvm::raw_ostream &os) override{
            os << llvm::raw_ostream::RED << "ERROR: " << llvm::raw_ostream::RESET << message << "\n";
        };
        SemanticADiagnostic(Ty type,const llvm::formatv_object_base & message,ASTStmt *stmt):type(type),stmt(stmt),message(message){
            
        };
        SemanticADiagnostic(Ty type,ASTStmt *stmt):type(type),stmt(stmt){
            /// Please Set Message!
        };
        ~SemanticADiagnostic(){
            
        };
    };

    struct ASTScopeSemanticsContext {
        ASTScope *scope;
        llvm::DenseMap<ASTIdentifier *,ASTType *> *args = nullptr;
    };

    class SemanticA {
        Syntax::SyntaxA & syntaxARef;
        DiagnosticBufferedLogger & errStream;
        bool typeExists(ASTType *type,Semantics::STableContext & symbolTableContext,ASTScope *scope);
        ASTType *evalExprForTypeId(ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext,ASTScopeSemanticsContext & scopeContext);
        bool typeMatches(ASTType *type,ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext,ASTScopeSemanticsContext & scopeContext);
        
        ASTType * evalGenericDecl(ASTDecl *stmt,Semantics::STableContext & symbolTableContext,ASTScopeSemanticsContext & scopeContext,bool * hasErrored);
        /**
            @param args Used with BlockStmts embedded in Function Decls.
        */
        ASTType * evalBlockStmtForASTType(ASTBlockStmt *block,Semantics::STableContext & symbolTableContext,bool * hasErrored,
        ASTScopeSemanticsContext & scopeContext,bool inFuncContext = false);
        bool checkSymbolsForStmtInScope(ASTStmt *stmt,Semantics::STableContext & symbolTableContext,
        ASTScope *scope,llvm::Optional<Semantics::SymbolTable> tempSTable = {});
    public:
        void start();
        void finish();
        void addSTableEntryForDecl(ASTDecl *decl,Semantics::SymbolTable *tablePtr);
        bool checkSymbolsForStmt(ASTStmt *stmt,Semantics::STableContext & symbolTableContext);
        SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticBufferedLogger & errStream);
    };
}

#endif
