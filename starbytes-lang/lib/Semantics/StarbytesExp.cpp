#include "StarbytesExp.h"
#include "starbytes/Semantics/SemanticsMain.h"

STARBYTES_SEMANTICS_NAMESPACE

std::string NUMBER = "Number";
std::string STRING = "String";
std::string BOOLEAN = "Boolean";

ExprStatement::ExprStatement(SemanticA *s):sem(s){

};

ExprStatement::~ExprStatement(){

};

void ExprStatement::visit(NODE *node){

};

STBType * evaluateASTExpressionStatement(ASTExpressionStatement *node_ty, SemanticA *sem){
    
};

STBType * evaluateASTCallExpression(ASTCallExpression *node_ty, SemanticA *sem){
    
};

STBType * evaluateASTStringLiteral(ASTStringLiteral *node_ty, SemanticA *sem){
    
};

STBType * evaluateASTNumericLiteral(ASTNumericLiteral *node_ty, SemanticA *sem){
    STBClassType * s;
    //TODO: CHANGE THIS TO GET SYMBOL FROM ALL CURRENT SCOPES!
    s = sem->store.getScopeRef(sem->store.exact_current_scope)->getSymbolRef<ClassSymbol>(NUMBER)->semantic_type;
    return s;
};

NAMESPACE_END