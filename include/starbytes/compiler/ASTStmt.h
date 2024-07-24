#include "ASTNodes.def"
#include <string>
#include <vector>
#include "starbytes/base/Diagnostic.h"

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
        std::shared_ptr<ASTScope> parentScope = nullptr;
        void generateHashID();
    };

    extern std::shared_ptr<ASTScope> ASTScopeGlobal;

    class ASTStmt {
    public:
        std::shared_ptr<ASTScope> scope;
        std::vector<Comment> beforeComments;
        std::vector<Comment> afterComments;
        ASTNodeType type;
        Region codeRegion;
        std::string parentFile;

        virtual void printToStream(std::ostream & out){};
        // virtual ~ASTStmt();
    };
}

#endif
