#include "ASTStmt.h"
#include <vector>

#ifndef STARBYTES_AST_ASTDECL_H
#define STARBYTES_AST_ASTDECL_H

namespace starbytes {

    class ASTDecl : public ASTStmt {
    public:
        struct Property {
            typedef enum : int {
                Identifier,
                Decl,
                Expr,
                Literal
            } Ty;
            Ty type;
            ASTStmt *dataPtr;
        };
        std::vector<Property> typeProps;
        std::vector<Property> declProps;
        static bool classof(ASTStmt *stmt);
    };

}

#endif
