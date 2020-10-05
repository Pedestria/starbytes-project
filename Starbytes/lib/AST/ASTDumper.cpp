#include "starbytes/AST/ASTDumper.h"

STARBYTES_STD_NAMESPACE

namespace AST {

    bool ASTDumper::toXMLDeclaration(ASTDeclaration *& decl){
        bool returncode = false;
        if(AST_NODE_IS(decl,ASTVariableDeclaration)){
            returncode = true;
        } else if(AST_NODE_IS(decl,ASTConstantDeclaration)){
            returncode = true;
        } else if(AST_NODE_IS(decl,ASTClassDeclaration)){
            returncode = true;
        } else if(AST_NODE_IS(decl,ASTInterfaceDeclaration)){
            returncode = true;
        }
        return returncode;
    };
    bool ASTDumper::toXMLExpression(ASTExpression *& expr){
        bool returncode = false;
        if(AST_NODE_IS(expr,ASTCallExpression)){
            returncode = true;
        } else if(AST_NODE_IS(expr,ASTArrayExpression)){
            returncode = true;
        } else if(AST_NODE_IS(expr,ASTAwaitExpression)){
            returncode = true;
        }
        return returncode;
    }
};

NAMESPACE_END