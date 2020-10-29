#include "starbytes/Semantics/SemanticsMain.h"
#include "starbytes/AST/AST.h"
#include "StarbytesDecl.h"
#include "StarbytesExp.h"
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

    void SemanticA::visitNode(AST::ASTNode * node,bool is_function,ASTNode *func_ptr){
        // std::cout << int(node->type) << " ;";
        try {
            if(is_function){
                if(AST_NODE_IS(node,ASTReturnDeclaration)){
                    std::cout << "Visiting Return Decl!";
                    ReturnDeclVisitor(this).visit(ASSERT_AST_NODE(node,ASTReturnDeclaration),func_ptr);
                }
            }
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
            else if(AST_NODE_IS(node,ASTExpressionStatement)){
                std::cout << "Visiting Expr Statement!" << std::endl;
                ExprStatementVisitor(this).visit(ASSERT_AST_NODE(node,ASTExpressionStatement));
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
        //TODO: Create Array class symbol with type args!
        std::cout << "Starting Semantics";
    }

    void SemanticA::analyzeFileForModule(Tree *& tree_ptr){
        tree = tree_ptr;
        for(auto & node : tree->nodes){
            visitNode(AST_NODE_CAST(node));
        }
    };

    void SemanticA::finish(){

    };

NAMESPACE_END