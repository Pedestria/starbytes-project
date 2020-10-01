
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
        Scope(){};
    };

    class ScopeStore {
        public:
        std::vector<Scope *> scopes;
        ScopeStore(){};
        ~ScopeStore(){};
    };
    
NAMESPACE_END



#endif
