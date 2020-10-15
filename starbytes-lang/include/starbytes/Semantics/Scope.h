
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
                        if(__sym->name == sym_2->name){
                            return true;
                            break;
                        }
                    }
                }
                return false;
            };
        Scope(std::string & _name):name(_name){};
        ~Scope(){};
    };

    class ScopeStore {
        private:
            std::vector<std::string> current_scopes;
            std::string exact_current_scope;
            friend class SemanticA;
        public:
            std::vector<Scope *> scopes;
        template<typename Lambda>
        void foreach(Lambda func);
        Scope *& getScopeRef(std::string &name);
        void setExactCurrentScope(std::string & __exact_current_scope);
        void addToCurrentScopes(Scope *& new_scope);
        void popCurrentScope();
        template<typename _SymbolTy>
        bool symbolExistsInCurrentScopes(std::string & symbol_name);
        ScopeStore(){};
        ~ScopeStore(){};
    };

    template<typename Lambda>
    void ScopeStore::foreach(Lambda func){
        for(auto & scope : scopes){
            func(scope);
        }
    };
    template<typename _SymbolTy>
    bool ScopeStore::symbolExistsInCurrentScopes(std::string &symbol_name){
        for(auto & _c_scope : current_scopes){
            _SymbolTy * sym = new _SymbolTy();
            sym->name = symbol_name;
            if(getScopeRef(_c_scope)->symbolExists(sym)){
                return true;
            };
        }
        return false;
    }   
    
NAMESPACE_END



#endif
