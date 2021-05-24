#include "ASTStmt.h"
#include "Type.h"
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/DenseMap.h>
#include <vector>

#ifndef STARBYTES_AST_ASTDECL_H
#define STARBYTES_AST_ASTDECL_H

namespace starbytes {

    class ASTExpr;

    /// Defines a new scope!!!
    struct ASTBlockStmt {
        ASTScope *parentScope;
        std::vector<ASTStmt *> body;
    };

    class ASTDecl : public ASTStmt {
        
    };

    class ASTImportDecl : public ASTDecl {
    public:
        ASTIdentifier *moduleName;
    };

    class ASTVarDecl : public ASTDecl {
    public:
        struct VarSpec {
            ASTIdentifier *id;
            ASTType *type = nullptr;
            /// Intializer for Variable;
            ASTExpr *expr = nullptr;
        };
        llvm::SmallVector<VarSpec,2> specs;
    };

    class ASTFuncDecl : public ASTDecl {
    public:
        ASTIdentifier *funcId;
        ASTType *returnType;
        llvm::DenseMap<ASTIdentifier *,ASTType *> params;
        ASTBlockStmt *blockStmt;
    };

}

#endif
