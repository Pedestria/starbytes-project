#include "starbytes/Semantics/Scope.h"
#include "starbytes/Semantics/StarbytesDecl.h"
#include "starbytes/Semantics/SemanticsMain.h"
#include "TypeCheck.h"
#include <iostream>

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;

//TODO: Implement!
ASTVisitorResponse atVarDecl(ASTTravelContext & context,SemanticA *sem){
    ASTVariableDeclaration *current = ASSERT_AST_NODE(context.current,ASTVariableDeclaration);
    // std::string baby;
    // sem->err_stream << makeSemanticADiagnostic(DiagnosticTy::Advice,"Test","TestFile",makeSrcLoc(baby,current->BeginFold));
    // // for(auto & spec : current->specifiers){

    // // };
    ASTVisitorResponse reply;
    reply.action = nullptr;
    reply.success = true;
    VISITOR_END
};
//TODO: Implement!
ASTVisitorResponse atConstDecl(ASTTravelContext & context,SemanticA *sem){
    ASTConstantDeclaration *current = ASSERT_AST_NODE(context.current,ASTConstantDeclaration);

    ASTVisitorResponse reply;
    reply.action = nullptr;
    reply.success = true;
    VISITOR_END
};
//TODO: Implement!
ASTVisitorResponse atFuncDecl(ASTTravelContext & context,SemanticA *sem){
    ASTFunctionDeclaration *current = ASSERT_AST_NODE(context.current,ASTFunctionDeclaration);

    ASTVisitorResponse reply;
    reply.action = nullptr;
    reply.success = true;
    VISITOR_END
};

NAMESPACE_END
