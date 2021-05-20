#include "starbytes/Syntax/SyntaxA.h"
#include <fstream>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/ErrorHandling.h>

#ifndef STARBYTES_PARSER_SEMANTICA_H
#define STARBYTES_PARSER_SEMANTICA_H

namespace starbytes {

    struct ASTScope;

    struct SemanticsContext {
        DiagnosticBufferedLogger & errStream;
        ASTStmt *currentStmt;
    };

    namespace Semantics  {
  

        struct SymbolTable {
            struct Entry {
                ASTStmt *node;
                std::string name;
                typedef enum : int {
                    Var,
                    Class,
                    Interface,
                    Scope,
                    Function
                } Ty;
                Ty type;
            };
        private:
            friend struct STableContext;
            llvm::DenseMap<Entry *,ASTScope *> body;
        public:
            void addSymbolInScope(Entry *entry,ASTScope * scope);
        };

        struct STableContext {
            std::unique_ptr<SymbolTable> main;
            std::vector<SymbolTable *> otherTables;
            bool hasTable(SymbolTable *ptr);
            SymbolTable::Entry * findEntry(llvm::StringRef symbolName,SemanticsContext & ctxt,ASTScope *scope = nullptr);
        };
    };

    class SemanticA {
        Syntax::SyntaxA & syntaxARef;
        DiagnosticBufferedLogger & errStream;
        ASTType *evalExprForTypeId(ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext);
        bool typeMatches(ASTType *type,ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext);
    public:
        void start();
        void finish();
        void addSTableEntryForDecl(ASTDecl *decl,Semantics::SymbolTable *tablePtr);
        bool checkSymbolsForStmt(ASTStmt *stmt,Semantics::STableContext & symbolTableContext);
        SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticBufferedLogger & errStream);
    };
}

#endif
