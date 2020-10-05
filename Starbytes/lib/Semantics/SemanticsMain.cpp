#include "starbytes/Semantics/SemanticsMain.h"
#include "starbytes/Semantics/Visitors/StarbytesDecl.h"
#include "starbytes/AST/AST.h"
#include <iostream>

namespace Starbytes {

namespace Semantics {

    void SemanticAnalyzer::initialize(){

        using namespace AST;

        SemanticSymbol *s = new FunctionSymbol();
        if(symbol_is<FunctionSymbol>(s)){
            std::cout << "Real Time TypeChecking!";
        }

        ScopeStore *Store;
        
        for(auto node : tree->nodes){
            if(AST_NODE_IS(node,ASTVariableDeclaration)){
                visitVariableDecl(ASSERT_AST_NODE(node,ASTVariableDeclaration),Store);
            }
        }
    }

}

}