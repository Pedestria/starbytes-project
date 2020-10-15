#include "starbytes/Semantics/SemanticsMain.h"
#include "starbytes/AST/AST.h"
#include "starbytes/Semantics/StarbytesDecl.h"
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
        if(AST_NODE_IS(node,ASTVariableDeclaration)){
            std::cout << "Visiting Variable Decl!";
            VariableDeclVisitor(this).visit(ASSERT_AST_NODE(node,ASTVariableDeclaration));
        }
    };

    void SemanticA::initialize(){
        std::string stdlib = "STARBYTES_GLOBAL";
        createScope(stdlib);
        store.setExactCurrentScope(stdlib);
        registerSymbolinExactCurrentScope(create_class_symbol("String"));
        std::cout << "Starting Semantics";
        for(auto & node : tree->nodes){
            visitNode(AST_NODE_CAST(node));
        }
    }

NAMESPACE_END