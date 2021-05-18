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
            code_var.hasInitValue = var_spec->declProps.size() > 1;
            code_var.id.value = var_id->val.c_str();
            code_var.id.len = var_id->val.size();
            genContext->out << &code_var;
            ASTExpr *initialVal = (ASTExpr *)var_spec->declProps[1].dataPtr;
            consumeStmt(initialVal);
        };
    };
};


void Gen::consumeStmt(ASTStmt *stmt){
    ASTExpr *expr = (ASTExpr *)stmt;
    if(stmt->type == STR_LITERAL){
        RTInternalObject ob;
        ob.isInternal = true;
        ob.type = RTINTOBJ_STR;
        RTInternalObject::StringParams *params = new RTInternalObject::StringParams();
        params->str = expr->literalValue;
        ob.data = params;
        genContext->out << &ob;
    }
    else if(stmt->type == ID_EXPR){
        RTCode code = CODE_RTVAR_REF;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        RTID id;
        llvm::StringRef func_str = expr->id->val;
        id.value = func_str.data();
        id.len = func_str.size();
        genContext->out << &id;
    }
    else if(stmt->type == IVKE_EXPR){
        RTCode code = CODE_RTIVKFUNC;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        RTID func_id;
        llvm::StringRef func_str = expr->id->val;
        func_id.value = func_str.data();
        func_id.len = func_str.size();
        genContext->out << &func_id;
        unsigned argCount = expr->exprArrayData.size();
        genContext->out.write((const char *)&argCount,sizeof(argCount));
        for(auto arg : expr->exprArrayData){
            consumeStmt(arg);
        };
    };
};


};
