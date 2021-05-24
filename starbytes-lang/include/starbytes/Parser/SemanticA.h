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

    struct SemanticADiagnostic : public Diagnostic {
        typedef enum : int {
          Error,
          Warning,
          Suggestion
        } Ty;
        Ty type;
        ASTStmt *stmt;
        std::string message;
        bool isError(){
            return type == Error;
        };
        void format(llvm::raw_ostream &os){
            os << llvm::raw_ostream::RED << "ERROR: " << llvm::raw_ostream::RESET << message << "\n";
        };
        SemanticADiagnostic(Ty type,const llvm::formatv_object_base & message,ASTStmt *stmt):type(type),message(message),stmt(stmt){
            
        };
        SemanticADiagnostic(Ty type,ASTStmt *stmt):type(type),stmt(stmt){
            /// Please Set Message!
        };
        ~SemanticADiagnostic(){
            
        };
    };

    class SemanticA {
        Syntax::SyntaxA & syntaxARef;
        DiagnosticBufferedLogger & errStream;
        bool typeExists(ASTType *type,Semantics::STableContext & symbolTableContext);
        ASTType *evalExprForTypeId(ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext);
        bool typeMatches(ASTType *type,ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext);
        /**
            @param args Used with BlockStmts embedded in Function Decls.
        */
        ASTType * evalBlockStmtForASTType(ASTBlockStmt *block,Semantics::STableContext & symbolTableContext,
        llvm::DenseMap<ASTIdentifier *,ASTType *> * args = nullptr,bool inFuncContext = false);
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
