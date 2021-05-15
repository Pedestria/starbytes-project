#include "starbytes/Gen/Gen.h"
#include "starbytes/RT/RTCode.h"

namespace starbytes {
ModuleGenContext::ModuleGenContext(llvm::StringRef strRef,std::ostream & out,Semantics::STableContext & tableContext):name(strRef),out(out),sTableContext(tableContext){
    
};

ModuleGenContext ModuleGenContext::Create(llvm::StringRef strRef,std::ostream & os,Semantics::STableContext & tableContext){
    return ModuleGenContext(strRef,os,tableContext);
};


Gen::Gen(DiagnosticBufferedLogger & errStream):errStream(errStream){
    
};

void Gen::setContext(ModuleGenContext *context){
    genContext = context;
};



void Gen::consumeDecl(ASTDecl *stmt){
    if(stmt->type == VAR_DECL){
        for(auto & prop : stmt->declProps){
            ASTDecl *var_spec = (ASTDecl *)prop.dataPtr;
            
        };
    };
};


void Gen::consumeStmt(ASTStmt *stmt){
    
};


};
