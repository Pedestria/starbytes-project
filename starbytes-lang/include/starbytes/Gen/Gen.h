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
        Semantics::STableContext * tableContext;
        static ModuleGenContext Create(llvm::StringRef strRef,std::ostream & out);
        ModuleGenContext(llvm::StringRef strRef,std::ostream & out);
    };

    class Gen : public ASTStreamConsumer {
        ModuleGenContext *genContext;
        DiagnosticBufferedLogger * errStream;
        friend class Parser;
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
