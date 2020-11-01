#include "StarbytesDecl.h"
#include "StarbytesExp.h"
#include "starbytes/Semantics/Scope.h"
#include "starbytes/Semantics/SemanticsMain.h"
#include "TypeCheck.h"
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
    // for(auto & specifier : node->specifiers){
    //     if(AST_NODE_IS(specifier->id,ASTTypeCastIdentifier)){
    //         ASTTypeCastIdentifier *tcid = ASSERT_AST_NODE(specifier->id,ASTTypeCastIdentifier);
    //         if(specifier->initializer.has_value()){
    //             if(sem->store.symbolExistsInCurrentScopes<ClassSymbol,ASTClassDeclaration>(tcid->tid->type_name)){
    //                 VariableSymbol *sym = new VariableSymbol(specifier);
    //                 sym->name = tcid->id->value;
    //                 SemanticSymbol *type_to_compare = evaluateASTExpression(specifier->initializer.value(),sem);
    //                 if(SEMANTIC_SYMBOL_IS(type_to_compare,ClassSymbol)){
    //                     ClassSymbol * class_ty = ASSERT_SEMANTIC_SYMBOL(type_to_compare,ClassSymbol);
    //                     ClassSymbol * test_val_ty = sem->store.getSymbolRefFromCurrentScopes<ClassSymbol>(tcid->tid->type_name);
    //                     if(class_ty->semantic_type->match(test_val_ty->semantic_type))  
    //                         sym->value_type = test_val_ty;
    //                     else
    //                        throw StarbytesSemanticsError("Type:`"+tcid->tid->type_name+"` does not match type: `"+class_ty->name,specifier->EndFold);
    //                 }
    //                 else
    //                   sem->registerSymbolinExactCurrentScope(sym);
    //                 continue;
    //             } 
    //             else 
    //               throw StarbytesSemanticsError("Unknown Type: "+tcid->tid->type_name,tcid->tid->BeginFold);
    //         }
    //         else{
    //             if(sem->store.symbolExistsInCurrentScopes<ClassSymbol,ASTClassDeclaration>(tcid->tid->type_name)){
    //                 VariableSymbol *sym = new VariableSymbol(specifier);
    //                 sym->name = tcid->id->value;
    //                 sem->registerSymbolinExactCurrentScope(sym);
    //                 continue;
    //             } 
    //             else {
    //                 throw StarbytesSemanticsError("Unknown Type: "+tcid->tid->type_name,tcid->tid->BeginFold);
    //             };
    //         }
    //     }
    //     else if(AST_NODE_IS(specifier->id,ASTIdentifier)){
    //         ASTIdentifier *id = ASSERT_AST_NODE(specifier->id,ASTIdentifier);
    //         ASTExpression * expr_to_eval = specifier->initializer.value();
    //         SemanticSymbol *type_to_compare = evaluateASTExpression(expr_to_eval,sem);
    //         VariableSymbol *sym = new VariableSymbol(specifier);
    //         sym->name = id->value;
    //         sym->value_type = type_to_compare;
    //         sem->registerSymbolinExactCurrentScope(sym);
    //         continue; 
    //     }
    // }
};

ConstantDeclVistior::ConstantDeclVistior(SemanticA *s):sem(s){

};

ConstantDeclVistior::~ConstantDeclVistior(){

};

void ConstantDeclVistior::visit(NODE *node){
    for(auto & specifier : node->specifiers){
        if(specifier->initializer.has_value()){
            ASTTypeCastIdentifier *tcid = specifier->id;
            if(sem->store.symbolExistsInCurrentScopes<ClassSymbol,ASTClassDeclaration>(tcid->tid->type_name)){
                ConstVariableSymbol *sym = new ConstVariableSymbol(specifier);
                sym->name = tcid->id->value;
                SemanticSymbol *type_to_compare = evaluateASTExpression(specifier->initializer.value(),sem);
                if(SEMANTIC_SYMBOL_IS(type_to_compare,ClassSymbol)){
                    ClassSymbol * class_ty = ASSERT_SEMANTIC_SYMBOL(type_to_compare,ClassSymbol);
                    ClassSymbol * test_val_ty = sem->store.getSymbolRefFromCurrentScopes<ClassSymbol>(tcid->tid->type_name);
                    if(class_ty->semantic_type->match(test_val_ty->semantic_type))  
                        sym->value_type = test_val_ty;
                    else
                    throw StarbytesSemanticsError("Type:`"+tcid->tid->type_name+"` does not match type: `"+class_ty->name,specifier->EndFold);
                }
                else
                  sem->registerSymbolinExactCurrentScope(sym);
                continue;
            } 
            else 
              throw StarbytesSemanticsError("Unknown Type: "+tcid->tid->type_name,tcid->tid->BeginFold);
        };
    };
}

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
        sem->visitNode(AST_NODE_CAST(body_statement),true);
    }
};

AcquireDeclVisitor::AcquireDeclVisitor(SemanticA *s):sem(s){

};

AcquireDeclVisitor::~AcquireDeclVisitor(){

};

void AcquireDeclVisitor::visit(NODE *node){

};

ReturnDeclVisitor::ReturnDeclVisitor(SemanticA *s):sem(s){

};

ReturnDeclVisitor::~ReturnDeclVisitor(){

};

void ReturnDeclVisitor::visit(NODE *node,ASTNode *& func_ptr){
    if(AST_NODE_IS(func_ptr,ASTFunctionDeclaration)){
        ASTFunctionDeclaration *parent = ASSERT_AST_NODE(func_ptr, ASTFunctionDeclaration);
        FunctionSymbol *& func_sym = sem->store.getSymbolRefFromCurrentScopes<FunctionSymbol>(parent->id->value);

        //TODO: Type check func return-type with expression type here!
        SemanticSymbol *ty = evaluateASTExpression(node->returnee,sem);

        if(stbtype_is_class(((ClassSymbol *)ty)->semantic_type) && stbtype_is_interface(func_sym->return_type)){
            if(((ClassSymbol *)ty)->semantic_type->match((STBClassType *)func_sym->return_type))
                return;
        }
    }
};

ClassDeclVisitor::ClassDeclVisitor(SemanticA *s):sem(s){

};

ClassDeclVisitor::~ClassDeclVisitor(){
    //sem->store.popCurrentScope();
};

void ClassDeclVisitor::visit(NODE *node){
    ClassSymbol *sym = new ClassSymbol(node);
    sym->name = node->id->value;
    sym->semantic_type = create_stb_class_type(node,sem);
    sem->registerSymbolinExactCurrentScope(sym);
};

NAMESPACE_END