#include "starbytes/Syntax/SyntaxA.h"
#include <fstream>
#include <llvm/ADT/DenseMap.h>

#ifndef STARBYTES_PARSER_SEMANTICA_H
#define STARBYTES_PARSER_SEMANTICA_H

namespace starbytes {
    namespace Semantics  {
        struct SymbolScope {
            std::string id;
            std::vector<SymbolScope *> childScopes;
        };

        struct SymbolTable {
            struct Entry {
                std::string name;
                typedef enum : int {
                    Var,
                    Class,
                    Interface,
                    Scope,
                } Ty;
                Ty type;
            };
        private:
            llvm::DenseMap<Entry *,SymbolScope *> body;
        };

        struct STableContext {
            std::vector<SymbolTable *> tables;
            bool hasTable(SymbolTable *ptr);
        };
    };

    std::ofstream & operator<<(std::ofstream & ostream,Semantics::SymbolTable & table);
    llvm::raw_ostream & operator<<(llvm::raw_ostream & ostream,Semantics::SymbolTable & table);
    std::ifstream & operator>>(std::ifstream & istream,Semantics::SymbolTable & table);

    class SemanticA {
        Syntax::SyntaxA & syntaxARef;
        DiagnosticBufferedLogger & errStream;
    public:
        void addSTableEntryForDecl(ASTDecl *decl,Semantics::SymbolTable *tablePtr);
        void checkSymbolsForStmt(ASTStmt *stmt,Semantics::STableContext & symbolTableContext);
        SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticBufferedLogger & errStream);
    };
}

#endif