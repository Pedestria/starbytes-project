#include "starbytes/Semantics/StarbytesDecl.h"
#include "starbytes/Semantics/Scope.h"
#include "starbytes/Semantics/SemanticsMain.h"
#include <iostream>

STARBYTES_SEMANTICS_NAMESPACE

void VariableDeclVisitor::visit(SemanticA *s,NODE *node){
    for(auto & specifier : node->specifiers){
        std::cout << "Varaible Decl IDType:" << int(specifier->id->type) << std::endl;
    }
    std::cout << "VariableDecl Name:" << ASSERT_AST_NODE(node->specifiers[0]->id,AST::ASTTypeCastIdentifier)->id->value << std::endl;
};

NAMESPACE_END