#include "starbytes/Semantics/SemanticsMain.h"
#include "starbytes/AST/AST.h"
#include "TypeCheck.h"
#include <iostream>


STARBYTES_SEMANTICS_NAMESPACE

    using namespace AST;

    ASTTravelerCallbackList<SemanticA> cb_list = {
                Foundation::dict_vec_entry(ASTType::VariableSpecifier,&atVarSpec),
                Foundation::dict_vec_entry(ASTType::FunctionDeclaration,&atFuncDecl)
            };

    SemanticA::SemanticA():ASTTraveler<SemanticA>(this,cb_list){};

    
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
        travel(tree);
    };

    void SemanticA::finish(){

    };

NAMESPACE_END