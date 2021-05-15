#include <string>
#include <fstream>
#include <llvm/ADT/StringRef.h>
#include "starbytes/Parser/SemanticA.h"
#include "starbytes/AST/AST.h"

#ifndef STARBYTES_GEN_H
#define STARBYTES_GEN_H

namespace starbytes {
    struct ModuleGenContext {
        std::string name;
        std::ostream & out;
        Semantics::STableContext & sTableContext;
        static ModuleGenContext Create(llvm::StringRef strRef,std::ostream & out,Semantics::STableContext & tableContext);
        ModuleGenContext(llvm::StringRef strRef,std::ostream & out,Semantics::STableContext & tableContext);
    };

    class Gen : public ASTStreamConsumer {
        ModuleGenContext *genContext;
        DiagnosticBufferedLogger & errStream;
    public:
        Gen(DiagnosticBufferedLogger & errStream);
        void setContext(ModuleGenContext *context);
        void consumeDecl(ASTDecl *stmt);
        void consumeStmt(ASTStmt *stmt);
    };
};

#endif
