#pragma once 
#include "AST/AST.h"
#include "Symbols/SymbolTable.h"

namespace Starbytes {

namespace Semantics {
        
    class SemanticAnalyzer {
        private:
            AST::AbstractSyntaxTree *tree;
            std::vector<SymbolTable *> tables;
            void createSymbolStore(std::string name,std::string scope);
        public:
            void freeSymbolStores();
            void analyze();
            SemanticAnalyzer(AST::AbstractSyntaxTree * _tree):tree(_tree){};
            ~SemanticAnalyzer();
    };
}

}