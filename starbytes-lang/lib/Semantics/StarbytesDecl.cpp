#include "starbytes/Semantics/StarbytesDecl.h"
#include "starbytes/Semantics/Scope.h"
#include "starbytes/Semantics/SemanticsMain.h"
#include <iostream>

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;

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
        if(AST_NODE_IS(specifier->id,ASTTypeCastIdentifier)){
            ASTTypeCastIdentifier *tcid = ASSERT_AST_NODE(specifier->id,ASTTypeCastIdentifier);
            if(sem->store.symbolExistsInCurrentScopes<ClassSymbol>(tcid->tid->type_name) == true){
                VariableSymbol *sym = new VariableSymbol();
                sym->name = tcid->id->value;
                sem->registerSymbolinExactCurrentScope(sym);
                continue;
            } 
            else {
                throw StarbytesSemanticsError("Unknown Type Reference: "+tcid->tid->type_name,tcid->tid->BeginFold);
            };
        }
        else if(AST_NODE_IS(specifier->id,ASTIdentifier)){

        }
    }
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