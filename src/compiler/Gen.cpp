#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/RTCode.h"

namespace starbytes {

using namespace Runtime;

ModuleGenContext::ModuleGenContext(string_ref strRef,std::ostream & out,std::filesystem::path & outputPath):name(strRef.str()),out(out),outputPath(outputPath),tableContext(nullptr){
    
};

ModuleGenContext ModuleGenContext::Create(string_ref strRef,std::ostream & os,std::filesystem::path & outputPath){
    return ModuleGenContext(strRef,os,outputPath);
};

Gen::Gen():
codeGen(std::make_unique<CodeGen>()),
interfaceGen(std::make_unique<InterfaceGen>())
{
    
};

void Gen::consumeSTableContext(Semantics::STableContext *ctxt){
    codeGen->consumeSTableContext(ctxt);
    if(interfaceEnabled){
        interfaceGen->consumeSTableContext(ctxt);
    }
};

void Gen::setContext(ModuleGenContext *context){
    genContext = context;
    codeGen->setContext(context);
    interfaceEnabled = (context && context->generateInterface);
    if(interfaceEnabled){
        interfaceGen->setContext(context);
    }
};

bool Gen::acceptsSymbolTableContext(){
    return codeGen->acceptsSymbolTableContext();
};

void Gen::consumeDecl(ASTDecl *stmt){
    codeGen->consumeDecl(stmt);
    if(interfaceEnabled){
        interfaceGen->consumeDecl(stmt);
    }
};

void Gen::consumeStmt(ASTStmt *stmt){
    codeGen->consumeStmt(stmt);
    if(interfaceEnabled){
        interfaceGen->consumeStmt(stmt);
    }
};

void Gen::finish(){
    codeGen->finish();
    if(interfaceEnabled){
        interfaceGen->finish();
    }
    if(genContext && genContext->tableContext && genContext->tableContext->main){
        std::ofstream out(std::filesystem::path(genContext->outputPath).append(genContext->name).concat(".starbsymtb"));
        genContext->tableContext->main->serializePublic(out);
        out.close();
    }
};




};
