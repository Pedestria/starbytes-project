#include "ASTDecl.h"
#include "ASTExpr.h"


#ifndef STARBYTES_AST_H
#define STARBYTES_AST_H

namespace starbytes {
    // struct ASTStreamDescriptor {
    //     size_t len;
    // };

    class ASTStreamConsumer {
        virtual void consumeStmt(ASTStmt *stmt) = 0;
        virtual void consumeDecl(ASTDecl *stmt) = 0;
    };
};

#endif

