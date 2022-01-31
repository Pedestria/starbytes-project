
#include "starbytes/AST/AST.h"
#include <fstream>

#ifndef STARBYTES_GEN_INTERFACEGEN_H
#define STARBYTES_GEN_INTERFACEGEN_H


namespace starbytes {

    struct ModuleGenContext;

    class InterfaceGen final : public ASTStreamConsumer {
        std::ofstream out;
        ModuleGenContext *genContext;
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
