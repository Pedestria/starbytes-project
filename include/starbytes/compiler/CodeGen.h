#include "SemanticA.h"
#include "AST.h"
#include "RTCode.h"
#include <set>

#ifndef STARBYTES_GEN_CODEGEN_H
#define STARBYTES_GEN_CODEGEN_H


namespace starbytes {

struct ModuleGenContext;

class CodeGen final : public ASTStreamConsumer {
    ModuleGenContext *genContext;
    DiagnosticHandler * errStream;
    size_t inlineFuncCounter = 0;
    size_t exprEmitDepth = 0;
    std::set<std::string> emittedInlineRuntimeFuncs;
    friend class Parser;
    StarbytesObject exprToRTInternalObject(ASTExpr *expr);
    void ensureInlineFunctionTemplate(ASTExpr *inlineExpr,const std::string &hint);
    void ensureInlineExprTemplates(ASTExpr *expr);
public:
    void finish();
    void consumeSTableContext(Semantics::STableContext *table) override;
    bool acceptsSymbolTableContext() override;
    void setContext(ModuleGenContext *context);
    void consumeDecl(ASTDecl *stmt) override;
    void consumeStmt(ASTStmt *stmt) override;
    ~CodeGen() = default;
};

}

#endif
