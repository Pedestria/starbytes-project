
#include "AST.h"
#include <memory>

#ifndef STARBYTES_AST_ASTDUMPER_H
#define STARBYTES_AST_ASTDUMPER_H

namespace starbytes {
    class ASTDumper {
        std::ostream & os;
        ASTDumper(std::ostream & os);
    public:
        static std::unique_ptr<ASTDumper> CreateStdoutASTDumper();
        static std::unique_ptr<ASTDumper> CreateOfstreamASTDumper(std::ofstream & out);
        void printStmt(ASTStmt *stmt,unsigned level);
        void printDecl(ASTDecl *decl,unsigned level);
    };
};

#endif