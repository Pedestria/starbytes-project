#include "AST/AST.h"
#include <initializer_list>

namespace Starbytes{
namespace AST {



void WalkVariableDeclaration(ASTVariableDeclaration *node,WalkPath<ASTVariableDeclaration> path,std::initializer_list<ASTCallbackEntry> *cb){

}

void WalkConstantDeclaration(ASTConstantDeclaration *node,WalkPath<ASTConstantDeclaration> path,std::initializer_list<ASTCallbackEntry> *cb){

}

void WalkStatement(ASTStatement *node,std::initializer_list<ASTCallbackEntry> *cb){
    switch (node->type) {
    case ASTType::VariableDeclaration:
        WalkVariableDeclaration((ASTVariableDeclaration *)node,WalkPath<ASTVariableDeclaration>(),cb);
        break;
    case ASTType::ConstantDeclaration :
        WalkConstantDeclaration((ASTConstantDeclaration *)node,WalkPath<ASTConstantDeclaration>(),cb);
        break;
    default:
        break;
    }
}

void WalkAST(AbstractSyntaxTree *tree, std::initializer_list<ASTCallbackEntry> callbacks){

    for(auto node : tree->nodes){
        WalkStatement(node,&callbacks);
    }

}


};
}

