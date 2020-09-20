#pragma once
#include <string>
#include <vector>
#include "AST/Document.h"

namespace Starbytes {

namespace Semantics {

    enum class SymbolType:int{
        Variable,Type,Function
    };

    struct Symbol {
        SymbolType type;
    };

    struct TypeSymbol : Symbol {
        std::string name;
        bool isArrayType;
        bool isGeneric;
        std::vector<std::string> typeargs;
        unsigned int array_count = 0;
    };

    struct VariableSymbol : Symbol{
        std::string value;
        DocumentPosition * start_pos;
        DocumentPosition * end_pos;
        //Reference to type this symbol has!
        TypeSymbol *typeref;
    };


}
}