#include "starbytes/AST/ASTStmt.h"


namespace starbytes {
    ASTScope * ASTScopeGlobal = new ASTScope({"GLOBAL",ASTScope::Neutral});

void ASTScope::generateHashID() {
    
    auto sha = llvm::SHA1();
    ASTScope *ps = parentScope;
    for(;ps != nullptr;ps = ps->parentScope){
        sha.update(ps->hash);
    }
    sha.update(name);
    hash = sha.final();
    
}
}
