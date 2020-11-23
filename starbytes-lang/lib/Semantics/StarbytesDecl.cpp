#include "starbytes/Semantics/Scope.h"
#include "starbytes/Semantics/StarbytesDecl.h"
#include "starbytes/Semantics/SemanticsMain.h"
#include "TypeCheck.h"
#include <iostream>

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;
//TODO: Implement!
ASTVisitorResponse atVarSpec(ASTTravelContext & context,SemanticA *sem){
    ASTVariableSpecifier *current = ASSERT_AST_NODE(context.current,ASTVariableSpecifier);
    ASTVisitorResponse reply;
    reply.action = nullptr;
    reply.success = true;
    return reply;
};
//TODO: Implement!
ASTVisitorResponse atConstSpec(ASTTravelContext & context,SemanticA *sem){
    ASTConstantSpecifier *current = ASSERT_AST_NODE(context.current,ASTConstantSpecifier);
    ASTVisitorResponse reply;
    reply.action = nullptr;
    reply.success = true;
    return reply;
};
//TODO: Implement!
ASTVisitorResponse atFuncDecl(ASTTravelContext & context,SemanticA *sem){
    ASTFunctionDeclaration *current = ASSERT_AST_NODE(context.current,ASTFunctionDeclaration);
    ASTVisitorResponse reply;
    reply.action = nullptr;
    reply.success = true;
    return reply;
};

NAMESPACE_END