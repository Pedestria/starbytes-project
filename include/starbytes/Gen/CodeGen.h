#include "starbytes/Parser/SemanticA.h"
#include "starbytes/AST/AST.h"
#include "starbytes/RT/RTCode.h"

#ifndef STARBYTES_GEN_CODEGEN_H
#define STARBYTES_GEN_CODEGEN_H


namespace starbytes {

struct ModuleGenContext;

class CodeGen final : public ASTStreamConsumer {
    ModuleGenContext *genContext;
    DiagnosticHandler * errStream;
    friend class Parser;
    StarbytesObject exprToRTInternalObject(ASTExpr *expr);
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
