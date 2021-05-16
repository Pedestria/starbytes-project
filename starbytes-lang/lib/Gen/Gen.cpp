#include "starbytes/Gen/Gen.h"
#include "starbytes/RT/RTCode.h"

namespace starbytes {

using namespace Runtime;

ModuleGenContext::ModuleGenContext(llvm::StringRef strRef,std::ostream & out):name(strRef),out(out){
    
};

ModuleGenContext ModuleGenContext::Create(llvm::StringRef strRef,std::ostream & os){
    return ModuleGenContext(strRef,os);
};

bool Gen::acceptsSymbolTableContext(){
    return genContext->tableContext != nullptr;
};

void Gen::consumeSTableContext(Semantics::STableContext *table){
    genContext->tableContext = table;
};

void Gen::setContext(ModuleGenContext *context){
    genContext = context;
};

void Gen::finish(){
    RTCode code = CODE_MODULE_END;
    genContext->out.write((char *)&code,sizeof(RTCode));
};



void Gen::consumeDecl(ASTDecl *stmt){
    if(stmt->type == VAR_DECL){
        for(auto & prop : stmt->declProps){
            ASTDecl *var_spec = (ASTDecl *)prop.dataPtr;
            ASTIdentifier *var_id = (ASTIdentifier *)var_spec->declProps[0].dataPtr;
            RTVar code_var;
            code_var.hasInitValue = false;
            code_var.id.value = var_id->val.c_str();
            code_var.id.len = var_id->val.size();
            genContext->out << code_var;
        };
    };
};


void Gen::consumeStmt(ASTStmt *stmt){
    
};


};
