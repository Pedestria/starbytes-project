#include "starbytes/AST/ASTExpr.h"

namespace starbytes {


bool ASTIdentifier::match(ASTIdentifier *other){
    return val == other->val;
}



}