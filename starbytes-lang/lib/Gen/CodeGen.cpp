#include "starbytes/Gen/CodeGen.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/RT/RTCode.h"

namespace starbytes {

using namespace Runtime;

bool CodeGen::acceptsSymbolTableContext(){
    return genContext->tableContext != nullptr;
};

void CodeGen::consumeSTableContext(Semantics::STableContext *table){
    genContext->tableContext = table;
};

void CodeGen::setContext(ModuleGenContext *context){
    genContext = context;
};

void CodeGen::finish(){
    RTCode code = CODE_MODULE_END;
    genContext->out.write((char *)&code,sizeof(RTCode));
};



void CodeGen::consumeDecl(ASTDecl *stmt){
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

bool stmtCanBeConvertedToRTInternalObject(ASTStmt *stmt){
    return (stmt->type == ARRAY_EXPR) || (stmt->type == DICT_EXPR) || (stmt->type & LITERAL);
};

Runtime::RTInternalObject * CodeGen::exprToRTInternalObject(ASTExpr *expr){
    RTInternalObject *ob = new RTInternalObject();
    ob->isInternal = true;
    if (expr->type == ARRAY_EXPR){
        ob->type = RTINTOBJ_ARRAY;
        RTInternalObject::ArrayParams *params = new RTInternalObject::ArrayParams();
        
        for(auto & expr : expr->exprArrayData){
            auto __expr = exprToRTInternalObject(expr);
            params->data.push_back(__expr);
        };
        
        ob->data = params;
    }
    else if(expr->type == STR_LITERAL){
        ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
        ob->type = RTINTOBJ_STR;
        RTInternalObject::StringParams *params = new RTInternalObject::StringParams();
        params->str = literal_expr->strValue.getValue();
        ob->data = params;
    }
    else if(expr->type == BOOL_LITERAL){
        ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
        ob->type = RTINTOBJ_BOOL;
        RTInternalObject::BoolParams *params = new RTInternalObject::BoolParams();
        params->value = literal_expr->boolValue.getValue();
        ob->data = params;
    }
    return ob;
};


void CodeGen::consumeStmt(ASTStmt *stmt){
    ASTExpr *expr = (ASTExpr *)stmt;
    /// Literals
    if(stmtCanBeConvertedToRTInternalObject(expr)){
        RTInternalObject *obj = exprToRTInternalObject(expr);
        genContext->out << obj;
        delete obj;
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
