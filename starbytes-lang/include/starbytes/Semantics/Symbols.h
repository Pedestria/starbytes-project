
#include "starbytes/Base/Base.h"

#ifndef SEMANTICS_SYMBOLS_H
#define SEMANTICS_SYMBOLS_H

STARBYTES_SEMANTICS_NAMESPACE
    enum class SymbolType:int {
        Variable,Class,Function
    }; 
    struct SemanticSymbol {
        SymbolType type;
        SemanticSymbol(SymbolType _type):type(_type){};
        ~SemanticSymbol();
    };

    struct VariableSymbol : public SemanticSymbol{
        VariableSymbol():SemanticSymbol(SymbolType::Variable){};
        ~VariableSymbol();
        static SymbolType stat_type;

    };

    struct ClassSymbol : public SemanticSymbol{
        ClassSymbol():SemanticSymbol(SymbolType::Class){};
        ~ClassSymbol();
        static SymbolType stat_type;
        
    };

    struct FunctionSymbol : public SemanticSymbol{
        FunctionSymbol():SemanticSymbol(SymbolType::Function){};
        ~FunctionSymbol();
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
    
NAMESPACE_END


#endif