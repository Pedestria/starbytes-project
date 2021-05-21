
#include "starbytes/AST/AST.h"

#ifndef STARBYTES_GEN_INTERFACEGEN_H
#define STARBYTES_GEN_INTERFACEGEN_H


namespace starbytes {

    class InterfaceGen : public ASTStreamConsumer {
    public:
        void consumeDecl(ASTDecl *stmt);
        void consumeStmt(ASTStmt *stmt);
    };

}

#endif
