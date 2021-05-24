#include "ASTStmt.h"
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallString.h>
#include "starbytes/Syntax/Toks.def"

#ifndef STARBYTES_AST_ASTEXPR_H
#define STARBYTES_AST_ASTEXPR_H

namespace starbytes {

    class ASTIdentifier : public ASTStmt  {
    public:
        std::vector<ASTIdentifier> genericArgs;
        std::string val;
        static bool classof(ASTStmt *stmt);
    };
    class ASTExpr : public ASTStmt {
    public:
        /// Identifier associated with Expr
        ASTIdentifier *id = nullptr;
        /// @name  Unary/Binary Expression Props
        /// @{

        /// The string of the operator
        llvm::Optional<llvm::SmallString<TOK_OP_MAX_LENGTH>> oprtr_str;
        /// @}

        /// @name Binary Expression Props
        /// @{
        ASTExpr * leftExpr = nullptr;

        ASTExpr * rightExpr = nullptr;
        /// @}

        /// ArrayExpr and InvokeExpr Props
        std::vector<ASTExpr *> exprArrayData;

        /// DictExpr Props
        llvm::DenseMap<ASTExpr *,ASTExpr *> dictExpr;
        static bool classof(ASTStmt *stmt);
    };

    class ASTLiteralExpr : public ASTExpr {
    public:
        llvm::Optional<std::string> strValue;
        
        llvm::Optional<bool> boolValue;
    };

};

#endif
