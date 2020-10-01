#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"
#include "starbytes/Semantics/Scope.h"

#ifndef SEMANTICS_SEMANTICS_MAIN_H
#define SEMANTICS_SEMANTICS_MAIN_H

STARBYTES_SEMANTICS_NAMESPACE
        
    class SemanticAnalyzer {
        private:
            AST::AbstractSyntaxTree *tree;
            ScopeStore store;
        public:
            void freeSymbolStores();
            void initialize();
            SemanticAnalyzer(AST::AbstractSyntaxTree * _tree):tree(_tree){};
            ~SemanticAnalyzer(){};
    };

NAMESPACE_END

#endif