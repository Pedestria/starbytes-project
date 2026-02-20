
#include "AST.h"
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef STARBYTES_GEN_INTERFACEGEN_H
#define STARBYTES_GEN_INTERFACEGEN_H


namespace starbytes {

    struct ModuleGenContext;

    class InterfaceGen final : public ASTStreamConsumer {
        std::ofstream out;
        ModuleGenContext *genContext;
        std::unordered_map<std::string,std::vector<std::string>> sourceLinesByPath;
        unsigned indentLevel = 0;

        const std::vector<std::string> &loadSourceLines(const std::string &path);
        std::vector<std::string> extractLeadingComments(ASTStmt *stmt);
        std::optional<std::string> renderLiteralExpr(ASTExpr *expr) const;
        std::string renderType(ASTType *type) const;
        bool lineUsesKeyword(ASTStmt *stmt,string_ref keyword);
        bool shouldEmitDecl(ASTDecl *stmt) const;
        void writeIndent();
        void emitDocComments(ASTStmt *stmt);
        void emitVarDecl(ASTVarDecl *node);
        void emitFuncDecl(ASTFuncDecl *node);
        void emitCtorDecl(ASTConstructorDecl *node);
        void emitClassDecl(ASTClassDecl *node);
        void emitInterfaceDecl(ASTInterfaceDecl *node);
        void emitScopeDecl(ASTScopeDecl *node);
        void emitTypeAliasDecl(ASTTypeAliasDecl *node);
        void emitDecl(ASTDecl *stmt);
    public:
        void setContext(ModuleGenContext *context);
        void finish();
        bool acceptsSymbolTableContext() override {
            return true;
        }
        void consumeSTableContext(Semantics::STableContext *ctxt) override;
        void consumeDecl(ASTDecl *stmt) override;
        void consumeStmt(ASTStmt *stmt) override;
        ~InterfaceGen() = default;
    };

}

#endif
