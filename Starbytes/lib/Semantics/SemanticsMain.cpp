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
        
        for(auto node : tree->nodes){
            if(astnode_is<ASTVariableDeclaration>((ASTNode *)node)){
                visitVariableDecl(cast_astnode<ASTVariableDeclaration *>(node),&store);
            }
        }
    }

}

}