#include <string>
#include <fstream>
#include "SemanticA.h"
#include "AST.h"
#include "RTCode.h"
#include "CodeGen.h"
#include "InterfaceGen.h"
#include <filesystem>

#ifndef STARBYTES_GEN_GEN_H
#define STARBYTES_GEN_GEN_H

namespace starbytes {
    struct ModuleGenContext {
        bool generateInterface = false;
        std::string name;
        std::ostream & out;
        std::filesystem::path outputPath;
        Semantics::STableContext * tableContext;
        static ModuleGenContext Create(string_ref strRef,std::ostream & out,std::filesystem::path & outputPath);
        ModuleGenContext(string_ref strRef,std::ostream & out,std::filesystem::path & outputPath);
    };

    class Gen : public ASTStreamConsumer {
        ModuleGenContext *genContext;
        DiagnosticHandler* errStream;
        friend class Parser;
        
        std::unique_ptr<CodeGen> codeGen;
        std::unique_ptr<InterfaceGen> interfaceGen;
        
    public:
        Gen();
        void finish();
        void consumeSTableContext(Semantics::STableContext *ctxt);
        bool acceptsSymbolTableContext();
        void setContext(ModuleGenContext *context);
        void consumeDecl(ASTDecl *stmt);
        void consumeStmt(ASTStmt *stmt);
    };
};

#endif
