#include "starbytes/AST/ASTDecl.h"

namespace starbytes {

bool ASTDecl::classof(ASTStmt *stmt){
    if(stmt->type | DECL){
        return true;
    }
    else {
        return false;
    };
};

};
