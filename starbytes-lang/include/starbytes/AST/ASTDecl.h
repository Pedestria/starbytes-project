#include "ASTStmt.h"
#include "Type.h"
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/DenseMap.h>
#include <vector>

#ifndef STARBYTES_AST_ASTDECL_H
#define STARBYTES_AST_ASTDECL_H

namespace starbytes {

    /// Defines a new scope!!!
    struct ASTBlockStmt {
        std::vector<ASTStmt *> body;
    };

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
        /// NOTE: Know the format of your Decls!
        std::vector<Property> declProps;
        
        llvm::Optional<ASTBlockStmt> blockStmt;
        
        ASTType *declType;
        
        static bool classof(ASTStmt *stmt);
    };

    class ASTFuncDecl : public ASTDecl {
    public:
        llvm::DenseMap<ASTIdentifier *,ASTType *> params;
    };

}

#endif
