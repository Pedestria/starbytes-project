#include "starbytes/Gen/CodeGen.h"
#include "starbytes/AST/ASTNodes.def"
#include "starbytes/Gen/Gen.h"
#include "starbytes/RT/RTCode.h"

namespace starbytes {

using namespace Runtime;

bool CodeGen::acceptsSymbolTableContext(){
    return genContext->tableContext != nullptr;
}

void CodeGen::consumeSTableContext(Semantics::STableContext *table){
    genContext->tableContext = table;
}

void CodeGen::setContext(ModuleGenContext *context){
    genContext = context;
}

void CodeGen::finish(){
    RTCode code = CODE_MODULE_END;
    genContext->out.write((char *)&code,sizeof(RTCode));
}

inline void write_ASTBlockStmt_to_context(ASTBlockStmt *blockStmt,ModuleGenContext *ctxt,CodeGen *astConsumer,bool isFunc = false){
    RTCode code;
    if(isFunc)
        code = CODE_RTFUNCBLOCK_BEGIN;
    else 
        code = CODE_RTBLOCK_BEGIN;
    ctxt->out.write((char *)&code,sizeof(RTCode));
    for(auto & stmt : blockStmt->body){
        if(stmt->type & DECL){
            astConsumer->consumeDecl((ASTDecl *)stmt);
        }
        else {
            astConsumer->consumeStmt(stmt);
        };
    };
    if(isFunc)
        code = CODE_RTFUNCBLOCK_END;
    else 
        code = CODE_RTBLOCK_END;
    ctxt->out.write((char *)&code,sizeof(RTCode));
}


inline void ASTIdentifier_to_RTID(ASTIdentifier *var,RTID &out){
    llvm::StringRef name = var->val;
    out.len = name.size();
    out.value = name.data();
}


void CodeGen::consumeDecl(ASTDecl *stmt){
    if(stmt->type == VAR_DECL){
        ASTVarDecl *varDecl = (ASTVarDecl *)stmt;
        for(auto & spec : varDecl->specs){
            ASTIdentifier *var_id = spec.id;
            RTVar code_var;
            ASTIdentifier_to_RTID(var_id,code_var.id);
            code_var.hasInitValue = (spec.expr != nullptr);
            genContext->out << &code_var;
            if(spec.expr) {
                ASTExpr *initialVal = spec.expr;
                consumeStmt(initialVal);
            }
        };
    }
    else if(stmt->type == FUNC_DECL){
        ASTFuncDecl *func_node = (ASTFuncDecl *)stmt;
        RTFuncTemplate funcTemplate;
        ASTIdentifier *func_id = func_node->funcId;
        ASTIdentifier_to_RTID(func_id,funcTemplate.name);
        for(auto & param_pair : func_node->params){
            RTID param_id;
            ASTIdentifier_to_RTID(param_pair.getFirst(),param_id);
            funcTemplate.argsTemplate.push_back(param_id);
        };
        genContext->out << &funcTemplate;
        write_ASTBlockStmt_to_context(func_node->blockStmt,genContext,this,true);
    }
    else if(stmt->type == COND_DECL){
        ASTConditionalDecl *cond_decl = (ASTConditionalDecl *)stmt;
        RTCode code = CODE_CONDITIONAL;
        genContext->out.write((char *)&code,sizeof(RTCode));
        unsigned count = cond_decl->specs.size();
        genContext->out.write((char *)&count,sizeof(count));
        for(auto & cond : cond_decl->specs){
            RTCode spec_ty = cond.isElse()? COND_TYPE_ELSE : COND_TYPE_IF;
            genContext->out.write((char *)&spec_ty,sizeof(RTCode));
            if(!cond.isElse())
                consumeStmt(cond.expr);
            write_ASTBlockStmt_to_context(cond.blockStmt,genContext,this);
        };
        code = CODE_CONDITIONAL_END;
        genContext->out.write((char *)&code,sizeof(RTCode));
    }
    else if(stmt->type == RETURN_DECL){
        ASTReturnDecl *return_decl = (ASTReturnDecl *)stmt;
        RTCode code = CODE_RTRETURN;
        genContext->out.write((char *)&code,sizeof(RTCode));
        bool hasValue = return_decl->expr != nullptr;
        genContext->out.write((char *)&hasValue,sizeof(hasValue));
        if(hasValue)
            consumeStmt(return_decl->expr);

    };
}

bool stmtCanBeConvertedToRTInternalObject(ASTStmt *stmt){
    return (stmt->type == ARRAY_EXPR) || (stmt->type == DICT_EXPR) || (stmt->type & LITERAL);
}

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
    else if(expr->type == NUM_LITERAL){
        ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
        ob->type = RTINTOBJ_NUM;
        RTInternalObject::NumberParams *params = new RTInternalObject::NumberParams();
        params->value = literal_expr->floatValue.hasValue() ? literal_expr->floatValue.getValue() : starbytes_float_t(literal_expr->intValue.getValue());
        ob->data = params;
    }
    return ob;
}


void CodeGen::consumeStmt(ASTStmt *stmt){
    ASTExpr *expr = (ASTExpr *)stmt;
    /// Literals
    if(stmtCanBeConvertedToRTInternalObject(expr)){
        RTInternalObject *obj = exprToRTInternalObject(expr);
        genContext->out << obj;
        delete obj;
    }
    else if(stmt->type == ID_EXPR){
        SemanticsContext ctxt {*errStream,stmt};
        
        if(expr->id->type == ASTIdentifier::Var) {
            RTCode code = CODE_RTVAR_REF;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            RTID id;
            ASTIdentifier_to_RTID(expr->id,id);
            genContext->out << &id;
        }
        else if(expr->id->type == ASTIdentifier::Function) {
            RTCode code = CODE_RTFUNC_REF;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            RTID id;
            ASTIdentifier_to_RTID(expr->id,id);
            std::cout << "ID Func Len:" << id.len << std::endl;
            genContext->out << &id;
        }
    }
    else if(stmt->type == IVKE_EXPR){
        /// Invoke Expr
        std::cout << "Invoke Expr" << std::endl;
        RTCode code = CODE_RTIVKFUNC;
        genContext->out.write((const char *)&code,sizeof(RTCode));
//        std::cout << "CALLEE TYPE:" << std::hex << expr->callee->type << std::dec << std::endl;
        consumeStmt(expr->callee);
        unsigned argCount = expr->exprArrayData.size();
        genContext->out.write((const char *)&argCount,sizeof(argCount));
        for(auto arg : expr->exprArrayData){
            consumeStmt(arg);
        };
    };
}


}
