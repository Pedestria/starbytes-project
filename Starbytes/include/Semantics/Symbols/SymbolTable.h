#pragma once
#include "Symbol.h"

namespace Starbytes {

namespace Semantics {

    class SymbolTable{
        public:
            std::string name;
            std::string scope;
            std::vector<Symbol *> symbols;
            void verifySymbolInTable(std::string name,SymbolTable type);
            void addSymbolToTable(Symbol *_symbol);
            SymbolTable(std::string _name,std::string _scope):name(_name),scope(_scope){};
    };

}

}