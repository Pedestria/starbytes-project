#include "ASTStmt.h"
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallString.h>
#include "starbytes/AST/ASTNodes.def"
#include "starbytes/Syntax/Toks.def"

#ifndef STARBYTES_AST_ASTEXPR_H
#define STARBYTES_AST_ASTEXPR_H

namespace starbytes {

    class ASTIdentifier : public ASTStmt  {
    public:
        std::string val;
        /// NOTE: Gets set by SemanticA
        typedef enum : int {
            Class,
            Function,
            Var
        } SymbolType;
        SymbolType type;
        static bool classof(ASTStmt *stmt);
        bool match(ASTIdentifier *other);
    };
    class ASTExpr : public ASTStmt {
    public:
        /// Identifier associated with Expr
        ASTIdentifier *id = nullptr;

        ASTExpr *callee;

        /// @name  Unary/Binary Expression Props
        /// @{

        /// The string of the operator
        llvm::Optional<llvm::SmallString<TOK_OP_MAX_LENGTH>> oprtr_str;
        /// @}

        /// @name Binary Expression / Member Expression Props
        /// @{
        ASTExpr * leftExpr = nullptr;
        /// NOTE: `rightExpr` is also used for AssignExpr
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

        llvm::Optional<starbytes_int_t> intValue;

        llvm::Optional<starbytes_float_t> floatValue;
    };

}

#endif
