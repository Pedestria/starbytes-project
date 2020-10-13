#include "starbytes/Semantics/Symbols.h"

STARBYTES_SEMANTICS_NAMESPACE

    #define SET_STATIC_TYPE(symbol_ty,enum_ty) SymbolType symbol_ty::stat_type = SymbolType::enum_ty

    SET_STATIC_TYPE(FunctionSymbol,Function);
    SymbolType VariableSymbol::stat_type = SymbolType::Variable;
    SymbolType ClassSymbol::stat_type = SymbolType::Class;
    
    bool VariableSymbol::checkWithOther(VariableSymbol *sym){
        bool returncode;
        if(this->name == sym->name){
            returncode = true;
        }
        else {
            returncode = false;
        }
        return returncode;
    };

NAMESPACE_END

