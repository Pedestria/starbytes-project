#include "ASTStmt.h"
#include "ASTNodes.def"
#include "Toks.def"

#ifndef STARBYTES_AST_ASTEXPR_H
#define STARBYTES_AST_ASTEXPR_H

namespace starbytes {
    struct ASTBlockStmt;
    class ASTType;

    class ASTIdentifier : public ASTStmt  {
    public:
        std::string val;
        /// NOTE: Gets set by SemanticA
        typedef enum : int {
            Class,
            Function,
            Var,
            Scope
        } SymbolType;
        SymbolType type;
        static bool classof(ASTStmt *stmt);
        bool match(ASTIdentifier *other);
    };
    class ASTExpr : public ASTStmt {
    public:
        bool isConstructorCall = false;
        bool isScopeAccess = false;
        std::shared_ptr<ASTScope> resolvedScope = nullptr;
        /// Identifier associated with Expr
        ASTIdentifier *id = nullptr;

        ASTExpr *callee;

        /// @name  Unary/Binary Expression Props
        /// @{

        /// The string of the operator
        std::optional<std::string> oprtr_str;
        std::optional<std::string> runtimeTypeCheckName;
        /// @}

        /// @name Binary Expression / Member Expression Props
        /// @{
        ASTExpr * leftExpr = nullptr;
        /// NOTE: `rightExpr` is also used for AssignExpr
        ASTExpr * rightExpr = nullptr;
        /// @}

        /// ArrayExpr and InvokeExpr Props
        std::vector<ASTExpr *> exprArrayData;
        std::vector<ASTType *> genericTypeArgs;

        /// DictExpr Props
        std::map<ASTExpr *,ASTExpr *> dictExpr;

        /// Inline Function Expr Props
        std::map<ASTIdentifier *,ASTType *> inlineFuncParams;
        ASTType *inlineFuncReturnType = nullptr;
        ASTBlockStmt *inlineFuncBlock = nullptr;
        std::string inlineFuncRuntimeName;
        static bool classof(ASTStmt *stmt);
    };

    class ASTLiteralExpr : public ASTExpr {
    public:
        std::optional<std::string> strValue;

        std::optional<std::string> regexPattern;

        std::optional<std::string> regexFlags;
        
        std::optional<bool> boolValue;

        std::optional<starbytes_int_t> intValue;

        std::optional<starbytes_float_t> floatValue;
    };

}

#endif
