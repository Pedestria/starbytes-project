#include "starbytes/Parser/SemanticA.h"

namespace starbytes {
    SemanticA::SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticBufferedLogger & errStream):syntaxARef(syntaxARef),errStream(errStream){

    };

    void SemanticA::addSTableEntryForDecl(ASTDecl *decl,Semantics::SymbolTable *tablePtr){

    };

    void SemanticA::checkSymbolsForStmt(ASTStmt *stmt,Semantics::STableContext & symbolTableContext){

    };
};

