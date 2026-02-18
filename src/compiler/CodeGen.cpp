#include "starbytes/compiler/CodeGen.h"
#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/RTCode.h"
#include <cstring>
#include <sstream>

namespace starbytes {

using namespace Runtime;

bool CodeGen::acceptsSymbolTableContext(){
    return genContext != nullptr;
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
    string_ref name = var->val;
    out.len = name.size();
    out.value = name.getBuffer();
}

static RTID makeOwnedRTID(const std::string &value){
    RTID id;
    id.len = value.size();
    char *buf = new char[id.len];
    std::memcpy(buf,value.data(),id.len);
    id.value = buf;
    return id;
}

static std::string mangleClassMethodName(const std::string &className,const std::string &methodName){
    return "__" + className + "__" + methodName;
}

static bool getMemberName(ASTExpr *memberExpr,std::string &memberName){
    if(!memberExpr || memberExpr->type != MEMBER_EXPR){
        return false;
    }
    if(!memberExpr->rightExpr || memberExpr->rightExpr->type != ID_EXPR || !memberExpr->rightExpr->id){
        return false;
    }
    memberName = memberExpr->rightExpr->id->val;
    return true;
}

static std::string emittedNameForScopeMember(ModuleGenContext *context,ASTExpr *memberExpr){
    if(!context || !context->tableContext || !memberExpr || !memberExpr->resolvedScope ||
       !memberExpr->rightExpr || !memberExpr->rightExpr->id){
        return {};
    }
    auto *entry = context->tableContext->findEntryInExactScopeNoDiag(memberExpr->rightExpr->id->val,memberExpr->resolvedScope);
    if(!entry){
        return {};
    }
    if(!entry->emittedName.empty()){
        return entry->emittedName;
    }
    return entry->name;
}

static std::vector<RTAttribute> convertAttributes(const std::vector<ASTAttribute> &astAttrs){
    std::vector<RTAttribute> out;
    out.reserve(astAttrs.size());
    for(const auto &astAttr : astAttrs){
        RTAttribute rtAttr;
        rtAttr.name = makeOwnedRTID(astAttr.name);
        for(const auto &astArg : astAttr.args){
            RTAttributeArg rtArg;
            rtArg.hasName = astArg.key.has_value();
            if(rtArg.hasName){
                rtArg.name = makeOwnedRTID(astArg.key.value());
            }

            auto *expr = (ASTExpr *)astArg.value;
            if(expr && expr->type == STR_LITERAL){
                auto *literal = (ASTLiteralExpr *)expr;
                rtArg.valueType = RTATTR_VALUE_STRING;
                rtArg.value = makeOwnedRTID(literal->strValue.value_or(""));
            }
            else if(expr && expr->type == BOOL_LITERAL){
                auto *literal = (ASTLiteralExpr *)expr;
                rtArg.valueType = RTATTR_VALUE_BOOL;
                rtArg.value = makeOwnedRTID(literal->boolValue.value_or(false) ? "true" : "false");
            }
            else if(expr && expr->type == NUM_LITERAL){
                auto *literal = (ASTLiteralExpr *)expr;
                rtArg.valueType = RTATTR_VALUE_NUMBER;
                if(literal->intValue.has_value()){
                    rtArg.value = makeOwnedRTID(std::to_string(literal->intValue.value()));
                }
                else {
                    rtArg.value = makeOwnedRTID(std::to_string(literal->floatValue.value_or(0.0)));
                }
            }
            else if(expr && expr->type == ID_EXPR && expr->id){
                rtArg.valueType = RTATTR_VALUE_IDENTIFIER;
                rtArg.value = makeOwnedRTID(expr->id->val);
            }
            else {
                rtArg.valueType = RTATTR_VALUE_IDENTIFIER;
                rtArg.value = makeOwnedRTID("");
            }
            rtAttr.args.push_back(std::move(rtArg));
        }
        out.push_back(std::move(rtAttr));
    }
    return out;
}

/*
 Generates runtime code. 
 (Breaks down complex/nested expressions to optimize runtime.)
*/


void CodeGen::consumeDecl(ASTDecl *stmt){
    if(stmt->type == VAR_DECL){
        ASTVarDecl *varDecl = (ASTVarDecl *)stmt;
        for(auto & spec : varDecl->specs){
            ASTIdentifier *var_id = spec.id;
            RTVar code_var;
            ASTIdentifier_to_RTID(var_id,code_var.id);
            code_var.hasInitValue = (spec.expr != nullptr);
            code_var.attributes = convertAttributes(varDecl->attributes);
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
        funcTemplate.attributes = convertAttributes(func_node->attributes);
        for(auto & param_pair : func_node->params){
            RTID param_id;
            ASTIdentifier_to_RTID(param_pair.first,param_id);
            funcTemplate.argsTemplate.push_back(param_id);
        };
        genContext->out << &funcTemplate;
        write_ASTBlockStmt_to_context(func_node->blockStmt,genContext,this,true);
    }
    else if(stmt->type == CLASS_DECL){
        auto class_decl = (ASTClassDecl *)stmt;
        RTClass cls;
        ASTIdentifier_to_RTID(class_decl->id,cls.name);
        cls.attributes = convertAttributes(class_decl->attributes);
        std::string className = class_decl->id->val;
        for(auto &fieldDecl : class_decl->fields){
            for(auto &spec : fieldDecl->specs){
                RTVar fieldVar;
                ASTIdentifier_to_RTID(spec.id,fieldVar.id);
                fieldVar.hasInitValue = false;
                fieldVar.attributes = convertAttributes(fieldDecl->attributes);
                cls.fields.push_back(std::move(fieldVar));
            }
        }
        for(auto &methodDecl : class_decl->methods){
            RTFuncTemplate methodTemplate;
            ASTIdentifier_to_RTID(methodDecl->funcId,methodTemplate.name);
            methodTemplate.attributes = convertAttributes(methodDecl->attributes);
            for(auto &paramPair : methodDecl->params){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                methodTemplate.argsTemplate.push_back(paramId);
            }
            cls.methods.push_back(std::move(methodTemplate));
        }
        genContext->out << &cls;

        for(auto &methodDecl : class_decl->methods){
            RTFuncTemplate runtimeMethod;
            runtimeMethod.name = makeOwnedRTID(mangleClassMethodName(className,methodDecl->funcId->val));
            runtimeMethod.attributes = convertAttributes(methodDecl->attributes);
            for(auto &paramPair : methodDecl->params){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                runtimeMethod.argsTemplate.push_back(paramId);
            }
            genContext->out << &runtimeMethod;
            write_ASTBlockStmt_to_context(methodDecl->blockStmt,genContext,this,true);
        }
    }
    else if(stmt->type == SCOPE_DECL){
        auto *scopeDecl = (ASTScopeDecl *)stmt;
        for(auto *innerStmt : scopeDecl->blockStmt->body){
            if(innerStmt->type & DECL){
                consumeDecl((ASTDecl *)innerStmt);
            }
            else {
                consumeStmt(innerStmt);
            }
        }
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

StarbytesObject CodeGen::exprToRTInternalObject(ASTExpr *expr){
    StarbytesObject ob;
    if (expr->type == ARRAY_EXPR){
        ob = StarbytesArrayNew();
        
        for(auto & expr : expr->exprArrayData){
            auto obj = exprToRTInternalObject(expr);
            StarbytesArrayPush(ob,obj);
        };
    }
    else if(expr->type == STR_LITERAL){
        ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
        ob = StarbytesStrNewWithData(literal_expr->strValue->data());
    }
    else if(expr->type == BOOL_LITERAL){
        ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
        ob = StarbytesBoolNew((StarbytesBoolVal)*literal_expr->boolValue);
    }
    else if(expr->type == NUM_LITERAL){
        ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
        if(literal_expr->floatValue.has_value()){
            ob = StarbytesNumNew(NumTypeFloat,*literal_expr->floatValue);
        }
        else {
            ob = StarbytesNumNew(NumTypeInt,*literal_expr->intValue);
        }
    }
    return ob;
}


void CodeGen::consumeStmt(ASTStmt *stmt){
    ASTExpr *expr = (ASTExpr *)stmt;
    /// Literals
    if(stmtCanBeConvertedToRTInternalObject(expr)){
        auto obj = exprToRTInternalObject(expr);
        genContext->out << &obj;
        StarbytesObjectRelease(obj);
    }
    else if(stmt->type == ID_EXPR){
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
            genContext->out << &id;
        }
    }
    else if(stmt->type == MEMBER_EXPR){
        if(expr->isScopeAccess){
            auto emittedName = emittedNameForScopeMember(genContext,expr);
            if(emittedName.empty()){
                return;
            }
            RTID memberId = makeOwnedRTID(emittedName);
            if(expr->rightExpr && expr->rightExpr->id && expr->rightExpr->id->type == ASTIdentifier::Function){
                RTCode code = CODE_RTFUNC_REF;
                genContext->out.write((const char *)&code,sizeof(RTCode));
                genContext->out << &memberId;
                return;
            }
            if(expr->rightExpr && expr->rightExpr->id && expr->rightExpr->id->type == ASTIdentifier::Var){
                RTCode code = CODE_RTVAR_REF;
                genContext->out.write((const char *)&code,sizeof(RTCode));
                genContext->out << &memberId;
                return;
            }
            return;
        }
        std::string memberName;
        if(!getMemberName(expr,memberName)){
            return;
        }
        RTCode code = CODE_RTMEMBER_GET;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        consumeStmt(expr->leftExpr);
        RTID memberId = makeOwnedRTID(memberName);
        genContext->out << &memberId;
    }
    else if(stmt->type == ASSIGN_EXPR){
        if(!expr->leftExpr || expr->leftExpr->type != MEMBER_EXPR){
            return;
        }
        std::string memberName;
        if(!getMemberName(expr->leftExpr,memberName)){
            return;
        }
        RTCode code = CODE_RTMEMBER_SET;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        consumeStmt(expr->leftExpr->leftExpr);
        RTID memberId = makeOwnedRTID(memberName);
        genContext->out << &memberId;
        consumeStmt(expr->rightExpr);
    }
    else if(stmt->type == IVKE_EXPR){
        if(expr->isConstructorCall){
            std::string className;
            if(!expr->callee){
                return;
            }
            if(expr->callee->type == ID_EXPR && expr->callee->id){
                className = expr->callee->id->val;
            }
            else if(expr->callee->type == MEMBER_EXPR && expr->callee->isScopeAccess){
                className = emittedNameForScopeMember(genContext,expr->callee);
            }
            if(className.empty()){
                return;
            }
            RTCode code = CODE_RTNEWOBJ;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            RTID classId = makeOwnedRTID(className);
            genContext->out << &classId;
            unsigned argCount = expr->exprArrayData.size();
            genContext->out.write((const char *)&argCount,sizeof(argCount));
            for(auto *arg : expr->exprArrayData){
                consumeStmt(arg);
            }
            return;
        }

        if(expr->callee && expr->callee->type == MEMBER_EXPR && !expr->callee->isScopeAccess){
            std::string memberName;
            if(!getMemberName(expr->callee,memberName)){
                return;
            }
            RTCode code = CODE_RTMEMBER_IVK;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            consumeStmt(expr->callee->leftExpr);
            RTID memberId = makeOwnedRTID(memberName);
            genContext->out << &memberId;
            unsigned argCount = expr->exprArrayData.size();
            genContext->out.write((const char *)&argCount,sizeof(argCount));
            for(auto *arg : expr->exprArrayData){
                consumeStmt(arg);
            }
            return;
        }

        RTCode code = CODE_RTIVKFUNC;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        consumeStmt(expr->callee);
        unsigned argCount = expr->exprArrayData.size();
        genContext->out.write((const char *)&argCount,sizeof(argCount));
        for(auto *arg : expr->exprArrayData){
            consumeStmt(arg);
        }
    }
    else {
        
    };
}


}
