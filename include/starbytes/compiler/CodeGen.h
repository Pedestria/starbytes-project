#include "SemanticA.h"
#include "AST.h"
#include "RTCode.h"
#include <map>
#include <set>
#include <vector>

#ifndef STARBYTES_GEN_CODEGEN_H
#define STARBYTES_GEN_CODEGEN_H


namespace starbytes {

struct ModuleGenContext;

class CodeGen final : public ASTStreamConsumer {
public:
    struct FastTypeInfo {
        Runtime::RTTypedNumericKind scalarKind = RTTYPED_NUM_OBJECT;
        Runtime::RTTypedNumericKind arrayElementKind = RTTYPED_NUM_OBJECT;
    };

    class BytecodeV2Lowerer;

private:
    struct LocalSlotContext {
        std::map<std::string,FastTypeInfo> slotTypeMap;
        std::map<std::string,ASTType *> slotSemanticTypeMap;
        std::map<std::string,uint32_t> slotMap;
        std::vector<Runtime::RTID> localSlotNames;
        std::vector<Runtime::RTTypedNumericKind> slotKinds;
        unsigned argSlotCount = 0;
    };

    ModuleGenContext *genContext;
    DiagnosticHandler * errStream;
    size_t inlineFuncCounter = 0;
    size_t exprEmitDepth = 0;
    std::set<std::string> emittedInlineRuntimeFuncs;
    std::vector<LocalSlotContext> localSlotStack;
    std::vector<ASTStmt *> bufferedTopLevelStatements;
    friend class Parser;
    StarbytesObject exprToRTInternalObject(ASTExpr *expr);
    FastTypeInfo inferFastType(ASTExpr *expr) const;
    ASTType *inferSemanticType(ASTExpr *expr) const;
    FastTypeInfo fastTypeFromTypeNode(ASTType *type) const;
    bool currentLocalSlotFastType(string_ref name,FastTypeInfo &typeOut) const;
    bool currentLocalSlotSemanticType(string_ref name,ASTType *&typeOut) const;
    bool tryResolveDirectFieldSlot(ASTExpr *memberExpr,uint32_t &slotOut) const;
    void ensureInlineFunctionTemplate(ASTExpr *inlineExpr,const std::string &hint);
    void ensureInlineExprTemplates(ASTExpr *expr);
public:
    void pushLocalSlotContext(const std::vector<std::pair<ASTIdentifier *,ASTType *>> &orderedParams,
                              ASTBlockStmt *blockStmt,
                              Runtime::RTFuncTemplate &templ);
    void popLocalSlotContext();
    bool currentLocalSlotForName(string_ref name,uint32_t &slotOut) const;
    void finish() const;
    void consumeSTableContext(Semantics::STableContext *table) override;
    bool acceptsSymbolTableContext() override;
    void setContext(ModuleGenContext *context);
    void consumeDecl(ASTDecl *stmt) override;
    void consumeStmt(ASTStmt *stmt) override;
    ~CodeGen() = default;
};

}

#endif
