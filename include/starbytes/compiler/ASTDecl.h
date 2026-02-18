#include "ASTStmt.h"
#include "Type.h"
#include <vector>
#include <optional>

#ifndef STARBYTES_AST_ASTDECL_H
#define STARBYTES_AST_ASTDECL_H

namespace starbytes {

    class ASTExpr;

    struct ASTAttributeArg {
        std::optional<std::string> key;
        ASTExpr *value = nullptr;
    };

    struct ASTAttribute {
        std::string name;
        std::vector<ASTAttributeArg> args;
    };

    /// Defines a new scope!!!
    struct ASTBlockStmt {
        std::shared_ptr<ASTScope> parentScope;
        std::vector<ASTStmt *> body;
    };

    class ASTDecl : public ASTStmt {
    public:
        std::vector<ASTAttribute> attributes;
    };

    class ASTImportDecl : public ASTDecl {
    public:
        ASTIdentifier *moduleName;
    };

    class ASTScopeDecl : public ASTDecl {
    public:
        ASTIdentifier *scopeId;
        ASTBlockStmt *blockStmt;
    };

    class ASTVarDecl : public ASTDecl {
    public:
        bool isConst = false;
        struct VarSpec {
            ASTIdentifier *id;
            ASTType *type = nullptr;
            /// Initializer for Variable;
            ASTExpr *expr = nullptr;
        };
        std::vector<VarSpec> specs;
    };

    class ASTFuncDecl : public ASTDecl {
    public:
        ASTIdentifier *funcId;
        ASTType *funcType;
        ASTType *returnType;
        std::map<ASTIdentifier *,ASTType *> params;
        ASTBlockStmt *blockStmt;
    };

    class ASTConstructorDecl : public ASTDecl {
    public:
        std::map<ASTIdentifier *,ASTType *> params;
        ASTBlockStmt *blockStmt;
    };

    class ASTClassDecl : public ASTDecl {
    public:

        ASTIdentifier *id;
        
        ASTType * classType;

        std::vector<ASTVarDecl *> fields;
        std::vector<ASTFuncDecl *> methods;
        std::vector<ASTConstructorDecl *> constructors;
    };

    class ASTReturnDecl : public ASTDecl {
    public:
        ASTExpr *expr;
    };

    class ASTConditionalDecl : public ASTDecl {
    public:
        struct CondDecl {
            ASTExpr *expr;
            ASTBlockStmt *blockStmt;
            bool isElse();
        };
        std::vector<CondDecl> specs;
    };

    class ASTForDecl : public ASTDecl {
    public:
        
    };

    class ASTWhileDecl : public ASTDecl {
    public:
        ASTExpr *expr;
        ASTBlockStmt *blockStmt;
    };

}

#endif
