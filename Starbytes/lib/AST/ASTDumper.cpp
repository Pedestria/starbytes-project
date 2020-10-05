#include "starbytes/AST/ASTDumper.h"

STARBYTES_STD_NAMESPACE

namespace AST {

    void ASTDumper::toXMLStatement(ASTStatement *& st){
        
    }

    void ASTDumper::toXMLDeclaration(ASTDeclaration *& decl){
        if(AST_NODE_IS(decl,ASTVariableDeclaration)){

        }
    };
};

NAMESPACE_END