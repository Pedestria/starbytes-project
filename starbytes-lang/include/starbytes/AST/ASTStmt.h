#include "ASTNodes.def"
#include <string>
#include <vector>

#ifndef STARBYTES_AST_ASTSTMT_H
#define STARBYTES_AST_ASTSTMT_H

namespace starbytes {

    struct SrcLoc {
        unsigned startCol;
        unsigned startLine;
        unsigned endCol;
        unsigned endLine;
    };

    struct Comment {
        typedef enum : int {
            Line,
            Block
        } CommentTy;
        std::string val;
        CommentTy type;
    };

    struct ASTScope {
        ASTScope *parentScope;
        std::vector<ASTScope *> childScopes;
    };

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