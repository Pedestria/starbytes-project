#include "starbytes/Semantics/Symbols.h"

namespace Starbytes {

namespace Semantics {
    SymbolType FunctionSymbol::stat_type = SymbolType::Function;
    SymbolType VariableSymbol::stat_type = SymbolType::Variable;
    SymbolType ClassSymbol::stat_type = SymbolType::Class;
    
};

}
