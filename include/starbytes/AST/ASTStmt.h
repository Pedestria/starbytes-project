#include "ASTNodes.def"
#include "starbytes/Base/CodeView.h"
#include <string>
#include <vector>

#ifndef STARBYTES_AST_ASTSTMT_H
#define STARBYTES_AST_ASTSTMT_H

namespace starbytes {

    struct Comment {
        typedef enum : int {
            Line,
            Block
        } CommentTy;
        std::string val;
        CommentTy type;
    };

    struct ASTScope {
        std::string name;
        typedef enum : int {
            Neutral,
            Namespace,
            Function,
        } ScopeType;
        ScopeType type;
        ASTScope *parentScope = nullptr;
    };

    extern ASTScope * ASTScopeGlobal;

    class ASTStmt {
    public:
        ASTScope * scope;
        std::vector<Comment *> beforeComments;
        std::vector<Comment *> afterComments;
        ASTNodeType type;
        SrcLoc loc;
        // virtual ~ASTStmt();
    };
};

#endif
