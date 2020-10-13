#include "starbytes/Semantics/Scope.h"

STARBYTES_SEMANTICS_NAMESPACE

void Scope::addSymbol(SemanticSymbol *& sym){
    symbols.push_back(sym);
};

NAMESPACE_END