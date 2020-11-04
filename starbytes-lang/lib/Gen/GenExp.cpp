#include "GenExp.h"

STARBYTES_GEN_NAMESPACE

using namespace AST;

void visitASTExpression(ASTExpression *_node, CodeGenR *_gen){
    if(AST_NODE_IS(_node,ASTCallExpression)){
        visitASTCallExpression(ASSERT_AST_NODE(_node,ASTCallExpression),_gen);
    }
    else if(AST_NODE_IS(_node,ASTArrayExpression)){
        visitASTArrayExpression(ASSERT_AST_NODE(_node,ASTArrayExpression),_gen);
    }
};

void visitASTArrayExpression(ASTArrayExpression *_node, CodeGenR *_gen){

};

void visitASTCallExpression(ASTCallExpression *_node, CodeGenR *_gen){

};

NAMESPACE_END