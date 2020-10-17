#include "StarbytesExp.h"
#include "starbytes/Semantics/SemanticsMain.h"

STARBYTES_SEMANTICS_NAMESPACE

std::string NUMBER = "Number";
std::string STRING = "String";
std::string BOOLEAN = "Boolean";

ExprStatementVisitor::ExprStatementVisitor(SemanticA *s):sem(s){

};

ExprStatementVisitor::~ExprStatementVisitor(){

};

void ExprStatementVisitor::visit(NODE *node){
    evaluateASTExpression(node->expression,sem);
};

STBType * evaluateASTExpression(ASTExpression *node_ty, SemanticA *sem){
    STBType *result;
    if(AST_NODE_IS(node_ty,ASTCallExpression)){
        result = evaluateASTCallExpression(ASSERT_AST_NODE(node_ty,ASTCallExpression),sem);
    }
    else if(AST_NODE_IS(node_ty,ASTBooleanLiteral)){
        result = evaluateASTBooleanLiteral(ASSERT_AST_NODE(node_ty,ASTBooleanLiteral),sem);
    }
    else if(AST_NODE_IS(node_ty,ASTStringLiteral)){
        result = evaluateASTStringLiteral(ASSERT_AST_NODE(node_ty,ASTStringLiteral),sem);
    }
    else if(AST_NODE_IS(node_ty,ASTNumericLiteral)){
        result = evaluateASTNumericLiteral(ASSERT_AST_NODE(node_ty,ASTNumericLiteral),sem);
    }
    else {
        result = nullptr;
    }
    return result;
};

STBType * evaluateASTArrayExpression(ASTArrayExpression *node_ty, SemanticA *sem){
    std::vector<STBType *> TYPE_ARRAY;
    for(auto & item : node_ty->items){
        TYPE_ARRAY.push_back(evaluateASTExpression(ASSERT_AST_NODE(item,ASTExpression),sem));
    }
    //TODO: Check each item in Array to see if this can infer the type array!
};

STBType * evaluateASTCallExpression(ASTCallExpression *node_ty, SemanticA *sem){
    ASTIdentifier *id;
    //TODO: Evaluate Identifiers of members to get symbol name!
    if(sem->store.symbolExistsInCurrentScopes<FunctionSymbol,ASTFunctionDeclaration>(id->value)){
        return sem->store.getSymbolRefFromCurrentScopes<FunctionSymbol>(id->value)->return_type;
    }
    else{
        throw StarbytesSemanticsError("Unknown Function:" + id->value,id->position);
    }
};



STBType * evaluateASTBooleanLiteral(ASTBooleanLiteral *node_ty, SemanticA *sem){
    return sem->store.getSymbolRefFromCurrentScopes<ClassSymbol>(BOOLEAN)->semantic_type;
};

STBType * evaluateASTStringLiteral(ASTStringLiteral *node_ty, SemanticA *sem){
    return sem->store.getSymbolRefFromCurrentScopes<ClassSymbol>(STRING)->semantic_type;
};

STBType * evaluateASTNumericLiteral(ASTNumericLiteral *node_ty, SemanticA *sem){
    STBClassType * s;
    s = sem->store.getSymbolRefFromCurrentScopes<ClassSymbol>(NUMBER)->semantic_type;
    return s;
};

NAMESPACE_END