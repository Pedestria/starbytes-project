#include "ASTNodes.def"
#include <string>
#include <vector>
#include "starbytes/Base/Diagnostic.h"

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
            Class
        } ScopeType;
        ScopeType type;
        ASTScope *parentScope = nullptr;
        // std::string hash = "NONE";
        // void generateHashID();
    };

    extern ASTScope * ASTScopeGlobal;

    class ASTStmt {
    public:
        std::shared_ptr<ASTScope> scope;
        std::vector<Comment> beforeComments;
        std::vector<Comment> afterComments;
        ASTNodeType type;
        Region codeRegion;
        std::string parentFile;
        // virtual ~ASTStmt();
    };
}

#endif
