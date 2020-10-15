
#include <vector>
#include "Symbols.h"
#include "starbytes/Base/Base.h"

#ifndef SEMANTICS_SCOPE_H
#define SEMANTICS_SCOPE_H

STARBYTES_SEMANTICS_NAMESPACE

    class Scope {
        private:
            std::vector<SemanticSymbol *> symbols;
        public:
            std::string & name;
            void addSymbol(SemanticSymbol *& sym);
            template<typename _SymbolTy>
            bool symbolExists(_SymbolTy *& sym_2){
                // _SymbolTy *sym = ASSERT_SEMANTIC_SYMBOL(sym_2,_SymbolTy);
                for(auto __sym : symbols){
                    if(SEMANTIC_SYMBOL_IS(__sym,_SymbolTy)){
                        
                    }
                }
            };
        Scope(std::string & _name):name(_name){};
        ~Scope(){};
    };

    class ScopeStore {
        private:
            std::vector<std::string> current_scopes;
            std::string exact_current_scope;
        public:
            std::vector<Scope *> scopes;
        template<typename Lambda>
        void foreach(Lambda func);
        Scope *& getScopeRef(std::string &name);
        void setExactCurrentScope(std::string & __exact_current_scope);
        void addToCurrentScopes(Scope *& new_scope);
        void popCurrentScope();
        ScopeStore(){};
        ~ScopeStore(){};
    };

    template<typename Lambda>
    void ScopeStore::foreach(Lambda func){
        for(auto & scope : scopes){
            func(scope);
        }
    };
    
NAMESPACE_END



#endif
