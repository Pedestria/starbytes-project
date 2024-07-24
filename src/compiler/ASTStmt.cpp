#include "starbytes/compiler/ASTStmt.h"


namespace starbytes {
    std::shared_ptr<ASTScope> ASTScopeGlobal = std::shared_ptr<ASTScope>(new ASTScope({"GLOBAL",ASTScope::Neutral}));

void ASTScope::generateHashID() {
    
    // auto sha = 100;
    // ASTScope *ps = parentScope;
    // for(;ps != nullptr;ps = ps->parentScope){
    //     sha.update(ps->hash);
    // }
    // sha.update(name);
    // hash = sha.final();
    
}
}
