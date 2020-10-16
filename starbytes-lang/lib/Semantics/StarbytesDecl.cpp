#include "StarbytesDecl.h"
#include "starbytes/Semantics/Scope.h"
#include "starbytes/Semantics/SemanticsMain.h"
#include <iostream>

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;

ImportDeclVisitor::ImportDeclVisitor(SemanticA *s):sem(s){

};

ImportDeclVisitor::~ImportDeclVisitor(){

};

void ImportDeclVisitor::visit(NODE *node){
    bool mod_found = false;
    for(auto & lib : sem->modules){
        if(lib == node->id->value){
            mod_found = true;
            break;
        }
    }
    if(!mod_found) {
        throw StarbytesSemanticsError("Cannot find Module: "+node->id->value,node->EndFold);
    }
};

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
            if(sem->store.symbolExistsInCurrentScopes<ClassSymbol,ASTClassDeclaration>(tcid->tid->type_name)){
                VariableSymbol *sym = new VariableSymbol(specifier);
                sym->name = tcid->id->value;
                sem->registerSymbolinExactCurrentScope(sym);
                continue;
            } 
            else {
                throw StarbytesSemanticsError("Unknown Type: "+tcid->tid->type_name,tcid->tid->BeginFold);
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
    FunctionSymbol *sym = new FunctionSymbol(node);
    sym->name = node->id->value;
    sem->registerSymbolinExactCurrentScope(sym);
    std::string name = node->id->value + "_FUNCTION";
    sem->createScope(name);
    for(auto & body_statement : node->body->nodes){
        sem->visitNode(AST_NODE_CAST(body_statement));
    }
};

ClassDeclVisitor::ClassDeclVisitor(SemanticA *s):sem(s){

};

ClassDeclVisitor::~ClassDeclVisitor(){
    sem->store.popCurrentScope();
};

void ClassDeclVisitor::visit(NODE *node){
    ClassSymbol *sym = new ClassSymbol(node);
    sym->name = node->id->value;
    sem->registerSymbolinExactCurrentScope(sym);

};

NAMESPACE_END