
#include <vector>
#include "Symbols.h"
#include "starbytes/Base/Base.h"

#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/Error.h>


#ifndef SEMANTICS_SCOPE_H
#define SEMANTICS_SCOPE_H

STARBYTES_SEMANTICS_NAMESPACE

using namespace llvm;

class SemanticA;

#define FRIEND_AST_EVALUATOR(type) friend SemanticSymbol * evaluate##type(type * node_ty,SemanticA *& sem)
    class Scope {
        private:
            std::vector<SemanticSymbol *> symbols; 
        public:
            std::string & name;
            void addSymbol(SemanticSymbol *& sym);
            template<typename Lambda>
            void foreach(Lambda callback);
            template<typename _SymbolTy>
            bool symbolExists(_SymbolTy *& sym_2){
                // _SymbolTy *sym = ASSERT_SEMANTIC_SYMBOL(sym_2,_SymbolTy);
                for(auto & __sym : symbols){
                    if(SEMANTIC_SYMBOL_IS(__sym,_SymbolTy)){
                        if(__sym->name == sym_2->name){
                            return true;
                            break;
                        }
                    }
                }
                return false;
            };
            template<typename SymTy>
            SymTy *& getSymbolRef(std::string & name);
            std::vector<SemanticSymbol *> & getIterator(){
                return symbols;
            };
        Scope(std::string & _name):name(_name){};
        ~Scope(){};
    };

    template<typename Lambda>
    void Scope::foreach(Lambda callback){
        for(auto & _sym : symbols){
            callback(_sym);
        }
    };

    template<typename SymTy>
    SymTy *& Scope::getSymbolRef(std::string & name){
        for(auto & __sym : symbols){
            if(SEMANTIC_SYMBOL_IS(__sym,SymTy)){
                if(__sym->name == name){
                    SymTy *ref = ASSERT_SEMANTIC_SYMBOL(__sym,SymTy);
                    return ref;
                    break;
                }
            }
        }
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
        Scope * getScopeRef(std::string &name);
        void setExactCurrentScope(std::string & __exact_current_scope);
        void addToCurrentScopes(Scope *& new_scope);
        void popCurrentScope();
        template<typename _SymbolTy,typename _PtrNodeTest>
        bool symbolExistsInCurrentScopes(std::string & symbol_name);
        template<typename SymTy>
        ErrorOr<SymTy *> getSymbolRefFromCurrentScopes(std::string & name);
        template<typename SymTy>
        ErrorOr<SymTy *> getSymbolRefFromExactCurrentScope(std::string & name);
        ScopeStore(){};
        ~ScopeStore(){};
    };

    template<typename Lambda>
    void ScopeStore::foreach(Lambda func){
        for(auto & scope : scopes){
            func(scope);
        }
    };
    /*`_PtrNodeTest` is just the type of node we need to create a mime symbol so we can check if the symbols exist or not!*/
    template<typename _SymbolTy,typename _PtrNodeTest>
    bool ScopeStore::symbolExistsInCurrentScopes(std::string &symbol_name){
        for(auto & _c_scope : current_scopes){
            _PtrNodeTest *ptr_test;
            _SymbolTy * sym = new _SymbolTy(ptr_test);
            sym->name = symbol_name;
            if(getScopeRef(_c_scope)->symbolExists(sym)){
                return true;
            };
        }
        return false;
    } 

    template<typename SymTy>
    ErrorOr<SymTy *> ScopeStore::getSymbolRefFromCurrentScopes(std::string & name){
        for(auto & _c_scope : current_scopes){
            Scope * scope_ref = getScopeRef(_c_scope);
            for(auto _sym : scope_ref->getIterator()){
                if(SEMANTIC_SYMBOL_IS(_sym,SymTy)){
                    if(_sym->name == name){
                        SymTy *ref = ASSERT_SEMANTIC_SYMBOL(_sym,SymTy);
                        return ref;
                    }
                }
            }
        }
    };

    template<typename SymTy>
    ErrorOr<SymTy *> ScopeStore::getSymbolRefFromExactCurrentScope(std::string &name){
        Scope * scope_ref = getScopeRef(exact_current_scope);
        for(auto & _sym : scope_ref->getIterator()){
            if(SEMANTIC_SYMBOL_IS(_sym,SymTy)){
                if(_sym->name == name){
                    SymTy *ref = ASSERT_SEMANTIC_SYMBOL(_sym,SymTy);
                    return ref;
                }
            }
        }
    };
    
NAMESPACE_END



#endif
