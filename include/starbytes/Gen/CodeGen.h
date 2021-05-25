#include "starbytes/Parser/SemanticA.h"
#include "starbytes/AST/AST.h"
#include "starbytes/RT/RTCode.h"

#ifndef STARBYTES_GEN_CODEGEN_H
#define STARBYTES_GEN_CODEGEN_H


namespace starbytes {

struct ModuleGenContext;

class CodeGen : public ASTStreamConsumer {
    ModuleGenContext *genContext;
    DiagnosticBufferedLogger * errStream;
    friend class Parser;
    Runtime::RTInternalObject *exprToRTInternalObject(ASTExpr *expr);
public:
    void finish();
    void consumeSTableContext(Semantics::STableContext *table);
    bool acceptsSymbolTableContext();
    void setContext(ModuleGenContext *context);
    void consumeDecl(ASTDecl *stmt);
    void consumeStmt(ASTStmt *stmt);
};

};

#endif
