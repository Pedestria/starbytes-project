#include "starbytes/Semantics/Visitors/StarbytesDecl.h"
#include "starbytes/Semantics/Scope.h"

STARBYTES_SEMANTICS_NAMESPACE

void visitVariableDecl(AST::ASTVariableDeclaration *node,ScopeStore *store){
    for(auto spec : node->specifiers){
        
    }
}

NAMESPACE_END