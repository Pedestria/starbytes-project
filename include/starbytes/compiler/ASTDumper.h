
#include "AST.h"
#include <memory>

#ifndef STARBYTES_AST_ASTDUMPER_H
#define STARBYTES_AST_ASTDUMPER_H

namespace starbytes {
    class ASTDumper final : public ASTStreamConsumer {
        std::ostream & os;
        ASTDumper(std::ostream & os);
        void printBlockStmt(ASTBlockStmt *blockStmt,unsigned level);
    public:
        static std::unique_ptr<ASTDumper> CreateStdoutASTDumper();
        static std::unique_ptr<ASTDumper> CreateOfstreamASTDumper(std::ofstream & out);
        bool acceptsSymbolTableContext() override;
        void consumeDecl(ASTDecl *stmt) override;
        void consumeStmt(ASTStmt *stmt) override;
        void printStmt(ASTStmt *stmt,unsigned level);
        void printDecl(ASTDecl *decl,unsigned level);
        ~ASTDumper() = default;
    };
}

#endif
