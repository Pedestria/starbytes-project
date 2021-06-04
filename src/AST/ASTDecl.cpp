#include "starbytes/AST/ASTDecl.h"

namespace starbytes {

bool ASTConditionalDecl::CondDecl::isElse(){
    return expr == nullptr;
}

//bool ASTDecl::classof(ASTStmt *stmt){
//    if(stmt->type | DECL){
//        return true;
//    }
//    else {
//        return false;
//    };
//};

};
