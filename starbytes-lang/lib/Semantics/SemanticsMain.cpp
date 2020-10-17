#include "starbytes/Semantics/SemanticsMain.h"
#include "starbytes/AST/AST.h"
#include "StarbytesDecl.h"
#include "TypeCheck.h"
#include <iostream>


STARBYTES_SEMANTICS_NAMESPACE

    using namespace AST;

    void SemanticA::createScope(std::string & name){
        Scope *scope = new Scope(name);
        store.addToCurrentScopes(scope);
    };

    void SemanticA::registerSymbolInScope(std::string & scope_name,SemanticSymbol * symbol){
        store.foreach([&scope_name,&symbol](Scope *& scope){
            if(scope->name == scope_name){
                scope->addSymbol(symbol);
            }
        });
    };

    void SemanticA::registerSymbolinExactCurrentScope(SemanticSymbol *symbol){
        for(auto & scope : store.scopes){
            if(scope->name == store.exact_current_scope){
                scope->addSymbol(symbol);
            }
        }
    };

    void SemanticA::visitNode(AST::ASTNode * node){
        // std::cout << int(node->type) << " ;";
        try {
            if(AST_NODE_IS(node,ASTImportDeclaration)){
                std::cout << "Visiting Import Decl!" << std::endl;
                ImportDeclVisitor(this).visit(ASSERT_AST_NODE(node,ASTImportDeclaration));
            }
            else if(AST_NODE_IS(node,ASTScopeDeclaration)){
                std::cout << "Visiting Scope Decl!" << std::endl;
                ScopeDeclVisitor(this).visit(ASSERT_AST_NODE(node,ASTScopeDeclaration));
            }
            else if(AST_NODE_IS(node,ASTVariableDeclaration)){
                std::cout << "Visiting Variable Decl!" << std::endl;
                VariableDeclVisitor(this).visit(ASSERT_AST_NODE(node,ASTVariableDeclaration));
            }
            else if(AST_NODE_IS(node,ASTClassDeclaration)){
                std::cout << "Visiting Class Decl!" << std::endl;
                ClassDeclVisitor(this).visit(ASSERT_AST_NODE(node,ASTClassDeclaration));
            }
        }
        catch(std::string error){
            std::cerr << "\x1b[31m" << error << "\x1b[0m" << std::endl;
        }
    };

    void SemanticA::initialize(){
        std::string stdlib = "STARBYTES_GLOBAL";
        createScope(stdlib);
        store.setExactCurrentScope(stdlib);
        ASTClassDeclaration *p;
        registerSymbolinExactCurrentScope(create_class_symbol("String",p,create_stb_standard_class_type("String")));
        registerSymbolinExactCurrentScope(create_class_symbol("Boolean",p,create_stb_standard_class_type("Boolean")));
        registerSymbolinExactCurrentScope(create_class_symbol("Number",p,create_stb_standard_class_type("Number")));
        std::cout << "Starting Semantics";
        for(auto & node : tree->nodes){
            visitNode(AST_NODE_CAST(node));
        }
    }

NAMESPACE_END