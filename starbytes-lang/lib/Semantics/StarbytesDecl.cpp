#include "starbytes/Semantics/StarbytesDecl.h"
#include "starbytes/Semantics/Scope.h"
#include "starbytes/Semantics/SemanticsMain.h"
#include <iostream>

STARBYTES_SEMANTICS_NAMESPACE

ScopeDeclVisitor::ScopeDeclVisitor(SemanticA *s):sem(s){

};

ScopeDeclVisitor::~ScopeDeclVisitor(){
    sem->store.popCurrentScope();
};

void ScopeDeclVisitor::visit(NODE *node){
    using namespace AST;
    sem->createScope(node->id->value);
    for(auto & each_item : node->body->nodes){
        sem->visitNode(AST_NODE_CAST(each_item));
    }
};

VariableDeclVisitor::VariableDeclVisitor(SemanticA *s):sem(s){

};

VariableDeclVisitor::~VariableDeclVisitor(){
    
};

void VariableDeclVisitor::visit(NODE *node){
    for(auto & specifier : node->specifiers){
        std::cout << "Varaible Decl IDType:" << int(specifier->id->type) << std::endl;
    }
    std::cout << "VariableDecl Name:" << ASSERT_AST_NODE(node->specifiers[0]->id,AST::ASTTypeCastIdentifier)->id->value << std::endl;
};

FunctionDeclVisitor::FunctionDeclVisitor(SemanticA *s):sem(s){
    
};

FunctionDeclVisitor::~FunctionDeclVisitor(){
    sem->store.popCurrentScope();
};

void FunctionDeclVisitor::visit(NODE *node){

};

ClassDeclVisitor::ClassDeclVisitor(SemanticA *s):sem(s){

};

ClassDeclVisitor::~ClassDeclVisitor(){
    sem->store.popCurrentScope();
};

void ClassDeclVisitor::visit(NODE *node){

};

NAMESPACE_END