#include "Semantics/Symbols/SymbolTable.h"

namespace Starbytes {

namespace Semantics {

    void SymbolTable::addSymbolToTable(Symbol * _symbol){
        symbols.push_back(_symbol);
    }

    void SymbolTable::verifySymbolInTable(std::string name,SymbolTable type){
        for(auto _sym : symbols){
            
        }
    }
}

}