#include "starbytes/Gen/Gen.h"
#include "starbytes/RT/RTCode.h"

namespace starbytes {

using namespace Runtime;

ModuleGenContext::ModuleGenContext(llvm::StringRef strRef,std::ostream & out):name(strRef),out(out){
    
};

ModuleGenContext ModuleGenContext::Create(llvm::StringRef strRef,std::ostream & os){
    return ModuleGenContext(strRef,os);
};

Gen::Gen():
codeGen(std::make_unique<CodeGen>())
{
    
};

void Gen::consumeSTableContext(Semantics::STableContext *ctxt){
    codeGen->consumeSTableContext(ctxt);
};

void Gen::setContext(ModuleGenContext *context){
    codeGen->setContext(context);
};

bool Gen::acceptsSymbolTableContext(){
    return codeGen->acceptsSymbolTableContext();
};

void Gen::consumeDecl(ASTDecl *stmt){
    codeGen->consumeDecl(stmt);
};

void Gen::consumeStmt(ASTStmt *stmt){
    codeGen->consumeStmt(stmt);
};

void Gen::finish(){
    codeGen->finish();
};




};

