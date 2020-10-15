#include "starbytes/Semantics/Scope.h"

STARBYTES_SEMANTICS_NAMESPACE

void Scope::addSymbol(SemanticSymbol *& sym){
    symbols.push_back(sym);
};

Scope *& ScopeStore::getScopeRef(std::string &name){
    for(auto & _scope : scopes){
        if(_scope->name == name){
            return _scope;
        }
    }
};

void ScopeStore::setExactCurrentScope(std::string &__exact_current_scope){
    exact_current_scope = __exact_current_scope;
};
/*Adds `new_scope` to current scopes plus stores it in long term memory!*/
void ScopeStore::addToCurrentScopes(Scope *&new_scope){
    scopes.push_back(new_scope);
    current_scopes.push_back(new_scope->name);
};
/*Pops scope out of `current_scopes` (Scope is still in long term memory), and sets current scope to latest scope.  */
void ScopeStore::popCurrentScope(){
    current_scopes.pop_back();
    setExactCurrentScope(current_scopes.back());
};

NAMESPACE_END