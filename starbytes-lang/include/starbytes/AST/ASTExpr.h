#include "ASTStmt.h"

#ifndef STARBYTES_AST_ASTEXPR_H
#define STARBYTES_AST_ASTEXPR_H

namespace starbytes {
    class ASTExpr : public ASTStmt {
    public:
        static bool classof(ASTStmt *stmt);
    };

    class ASTIdentifier : public ASTStmt  {
    public:
        std::vector<ASTIdentifier> genericArgs;
        std::string val;
        static bool classof(ASTStmt *stmt);
    };
};

#endif