
#include "starbytes/Base/Base.h"

#ifndef SEMANTICS_SYMBOLS_H
#define SEMANTICS_SYMBOLS_H

STARBYTES_SEMANTICS_NAMESPACE
    enum class SymbolType:int {
        Variable,Class,Function
    }; 
    struct SemanticSymbol {
        SymbolType type;
        std::string name;
        SemanticSymbol(SymbolType _type):type(_type){};
        //bool checkWithOther!
        ~SemanticSymbol(){};
    };

    class VariableSymbol : public SemanticSymbol{
        VariableSymbol():SemanticSymbol(SymbolType::Variable){};
        ~VariableSymbol(){};
        bool checkWithOther(VariableSymbol *sym);
        static SymbolType stat_type;

    };

    class ClassSymbol : public SemanticSymbol{
        ClassSymbol():SemanticSymbol(SymbolType::Class){};
        ~ClassSymbol(){};
        bool checkWithOther(ClassSymbol *sym);
        static SymbolType stat_type;
        
    };

    class FunctionSymbol : public SemanticSymbol{
        FunctionSymbol():SemanticSymbol(SymbolType::Function){};
        ~FunctionSymbol(){};
        bool checkWithOther(FunctionSymbol *sym);
        static SymbolType stat_type;
        
    };

    template<class _SymbolTy>
    bool symbol_is(SemanticSymbol *_symbol){
        if(_symbol->type == _SymbolTy::stat_type){
            return true;
        }
        else {
            return false;
        }
    }

    #define SEMANTIC_SYMBOL_IS(ptr,type) symbol_is<type>((SemanticSymbol *)ptr)
    #define ASSERT_SEMANTIC_SYMBOL(ptr,type) ((type *)ptr)
    
NAMESPACE_END


#endif