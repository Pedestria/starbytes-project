#include "Semantics/SemanticsEngine.h"
#include "Parser/AST.h"
#include "Parser/Document.h"
#include "Parser/Parser.h"
#include <string>
#include <vector>

using namespace Starbytes;

void SymbolStore::addSymbol(SymbolStoreEntry *symbol){
    symbols.push_back(symbol);
}

bool SymbolStore::verifySymbol(std::string symbol,SymbolType symbol_type){
    for(auto sym : symbols){
        if(sym->symbol == symbol && sym->type == symbol_type){
            return true;
        }
    }
    return false;
}


bool inStoreScope(SymbolStore * store,std::vector<std::string> *scopes){
    for(auto scope : *scopes){
        if(scope == store->name){
            return true;
        }
    }
    return false;
}


void SemanticAnalyzer::createSymbolStore(std::string name,Scope scope_type){
    stores.push_back(new SymbolStore(name,scope_type));
}

void SemanticAnalyzer::addSymbolToStore(std::string store_name,SymbolStoreEntry* symbol){
    for(auto store : stores){
        if(store->name == store_name){
            store->addSymbol(symbol);
            break;
        }
    }
}

void SemanticAnalyzer::verifySymbolInCurrentStores(std::string _symbol,SymbolType symbol_type,DocumentPosition *position){
    bool resolved = false;
    for(auto store : stores){
        if(inStoreScope(store,&currentStoreScopes)){
            if(store->verifySymbol(_symbol,symbol_type)){
                resolved = true;
            }
        }
    }
    if(!resolved){
        throw std::string("Unresolved Symbol! at Line:")+std::to_string(position->line);
    }
}

void SemanticAnalyzer::freeSymbolStores(){
    for(auto store : stores){
        auto symbols = *(store->getSymbols());
        symbols.clear();
        delete store;
    }
};

void SemanticAnalyzer::addToCurrentScopes(std::string scope_name){
    currentStoreScopes.push_back(scope_name);
}

void SemanticAnalyzer::removeFromCurrentScopes(std::string scope_name){
    for(int i = 0; i < currentStoreScopes.size();++i){
        if(currentStoreScopes[i] == scope_name){
            currentStoreScopes.erase(currentStoreScopes.begin()+i);
        }
    }
}


void SemanticAnalyzer::check(bool semanticTokens,std::vector<SemanticToken *> * semTokArray) {
    outputSemanticTok = semanticTokens;
    if(semTokArray && semanticTokens){
        semanticTokArray = semTokArray;
    }

    createSymbolStore("GLOBAL",Scope::Global);
    addToCurrentScopes("GLOBAL");

    for(auto node : syntaxTree->nodes){
        checkStatement(node);
    }

};

void SemanticAnalyzer::checkStatement(ASTStatement *node){
    switch (node->type) {
    case AST::ASTType::FunctionDeclaration:
        checkFunctionDeclaration((ASTFunctionDeclaration *)node);
        break;
    case AST::ASTType::ClassDeclaration :
        checkClassDeclaration((ASTClassDeclaration *) node);
        break;
    case AST::ASTType::ScopeDeclaration:
        checkScopeDeclaration((ASTScopeDeclaration *) node);
        break;
    default:
        throw "Parser Fault!";
        break;
    }
}
