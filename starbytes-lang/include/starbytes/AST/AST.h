#include "ASTDecl.h"
#include "ASTExpr.h"
#include <iostream>

#ifndef STARBYTES_AST_H
#define STARBYTES_AST_H

namespace starbytes {
    // struct ASTStreamDescriptor {
    //     size_t len;
    // };

    namespace Semantics {
        struct STableContext;
    }

    class ASTStreamConsumer {
    public:
        virtual bool acceptsSymbolTableContext() = 0;
        virtual void consumeSTableContext(Semantics::STableContext *table){
            std::cout << "NOT IMPLEMENTED" << std::endl;
        };
        virtual void consumeStmt(ASTStmt *stmt) = 0;
        virtual void consumeDecl(ASTDecl *stmt) = 0;
    };
};

#endif

