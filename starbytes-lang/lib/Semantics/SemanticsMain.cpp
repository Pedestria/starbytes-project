#include "starbytes/Semantics/SemanticsMain.h"
#include "starbytes/AST/AST.h"
#include "starbytes/Semantics/StarbytesDecl.h"
#include <iostream>


STARBYTES_SEMANTICS_NAMESPACE

    using namespace AST;

    void SemanticA::createScope(std::string & name){
        Scope *scope = new Scope(name);
        store.scopes.push_back(scope);
    };

    void SemanticA::registerSymbolInScope(std::string & scope_name,SemanticSymbol *& symbol){
        store.foreach([&scope_name,&symbol](Scope *& scope){
            if(scope->name == scope_name){
                scope->addSymbol(symbol);
            }
        });
    };

    void SemanticA::visitNode(AST::ASTNode *& node){
        // std::cout << int(node->type) << " ;";
        if(AST_NODE_IS(node,ASTVariableDeclaration)){
            std::cout << "Visiting Variable Decl!";
            VariableDeclVisitor().visit(this,ASSERT_AST_NODE(node,ASTVariableDeclaration));
        }
    };

    void SemanticA::initialize(){
        std::cout << "Starting Semantics";

        ScopeStore *Store;
        for(auto & node : tree->nodes){
            if(AST_NODE_IS(node,ASTVariableDeclaration)){
                std::cout << "Visiting Variable Decl!" << std::endl;
                VariableDeclVisitor().visit(this,ASSERT_AST_NODE(node,ASTVariableDeclaration));
            }
        }
    }

NAMESPACE_END