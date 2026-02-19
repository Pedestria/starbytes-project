#include "starbytes/compiler/CodeGen.h"
#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/RTCode.h"
#include <algorithm>
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
    (void)isFunc;
    RTCode code = CODE_RTBLOCK_BEGIN;
    ctxt->out.write((char *)&code,sizeof(RTCode));
    for(auto & stmt : blockStmt->body){
        if(stmt->type & DECL){
            astConsumer->consumeDecl((ASTDecl *)stmt);
        }
        else {
            astConsumer->consumeStmt(stmt);
        };
    };
    code = CODE_RTBLOCK_END;
    ctxt->out.write((char *)&code,sizeof(RTCode));
}

static std::string emitBlockBodyToBuffer(ASTBlockStmt *blockStmt,ModuleGenContext *ctxt,CodeGen *astConsumer){
    if(!blockStmt){
        return {};
    }
    std::ostringstream bodyBuffer(std::ios::out | std::ios::binary);
    auto tempOutputPath = ctxt->outputPath;
    ModuleGenContext tempContext(ctxt->name,bodyBuffer,tempOutputPath);
    tempContext.tableContext = ctxt->tableContext;
    astConsumer->setContext(&tempContext);
    for(auto *stmt : blockStmt->body){
        if(stmt->type & DECL){
            astConsumer->consumeDecl((ASTDecl *)stmt);
        }
        else {
            astConsumer->consumeStmt(stmt);
        }
    }
    astConsumer->setContext(ctxt);
    return bodyBuffer.str();
}

static void emitRuntimeFunction(ASTBlockStmt *blockStmt,ModuleGenContext *ctxt,CodeGen *astConsumer,RTFuncTemplate &templ){
    auto bodyBytes = emitBlockBodyToBuffer(blockStmt,ctxt,astConsumer);
    templ.blockByteSize = bodyBytes.size();
    ctxt->out << &templ;
    RTCode code = CODE_RTFUNCBLOCK_BEGIN;
    ctxt->out.write((char *)&code,sizeof(RTCode));
    if(!bodyBytes.empty()){
        ctxt->out.write(bodyBytes.data(),(std::streamsize)bodyBytes.size());
    }
    code = CODE_RTFUNCBLOCK_END;
    ctxt->out.write((char *)&code,sizeof(RTCode));
}

inline void ASTIdentifier_to_RTID(ASTIdentifier *var,RTID &out);
static RTID makeOwnedRTID(string_ref value);

static bool paramComesBefore(ASTIdentifier *lhs,ASTIdentifier *rhs){
    if(lhs == rhs){
        return false;
    }
    if(!lhs){
        return false;
    }
    if(!rhs){
        return true;
    }
    if(lhs->codeRegion.startLine != rhs->codeRegion.startLine){
        return lhs->codeRegion.startLine < rhs->codeRegion.startLine;
    }
    if(lhs->codeRegion.startCol != rhs->codeRegion.startCol){
        return lhs->codeRegion.startCol < rhs->codeRegion.startCol;
    }
    return lhs->val < rhs->val;
}

static std::vector<std::pair<ASTIdentifier *,ASTType *>> orderedParamPairs(const std::map<ASTIdentifier *,ASTType *> &paramMap){
    std::vector<std::pair<ASTIdentifier *,ASTType *>> ordered;
    ordered.reserve(paramMap.size());
    for(auto &entry : paramMap){
        ordered.push_back(entry);
    }
    std::sort(ordered.begin(),ordered.end(),[](const auto &lhs,const auto &rhs){
        return paramComesBefore(lhs.first,rhs.first);
    });
    return ordered;
}

static void collectInlineFunctionExprs(ASTExpr *expr,std::vector<ASTExpr *> &out){
    if(!expr){
        return;
    }
    if(expr->type == INLINE_FUNC_EXPR){
        out.push_back(expr);
        return;
    }
    if(expr->leftExpr){
        collectInlineFunctionExprs(expr->leftExpr,out);
    }
    if(expr->middleExpr){
        collectInlineFunctionExprs(expr->middleExpr,out);
    }
    if(expr->rightExpr){
        collectInlineFunctionExprs(expr->rightExpr,out);
    }
    if(expr->callee){
        collectInlineFunctionExprs(expr->callee,out);
    }
    for(auto *arg : expr->exprArrayData){
        collectInlineFunctionExprs(arg,out);
    }
    for(auto &entry : expr->dictExpr){
        collectInlineFunctionExprs(entry.first,out);
        collectInlineFunctionExprs(entry.second,out);
    }
}

void CodeGen::ensureInlineFunctionTemplate(ASTExpr *inlineExpr,const std::string &hint){
    if(!inlineExpr || inlineExpr->type != INLINE_FUNC_EXPR || !inlineExpr->inlineFuncBlock){
        return;
    }
    if(inlineExpr->inlineFuncRuntimeName.empty()){
        auto suffix = hint.empty() ? "inline" : hint;
        inlineExpr->inlineFuncRuntimeName = "__inline__" + suffix + "_" + std::to_string(inlineFuncCounter++);
    }
    if(emittedInlineRuntimeFuncs.find(inlineExpr->inlineFuncRuntimeName) != emittedInlineRuntimeFuncs.end()){
        return;
    }

    RTFuncTemplate inlineTemplate;
    inlineTemplate.name = makeOwnedRTID(inlineExpr->inlineFuncRuntimeName);
    auto orderedParams = orderedParamPairs(inlineExpr->inlineFuncParams);
    for(auto &paramPair : orderedParams){
        RTID paramId;
        ASTIdentifier_to_RTID(paramPair.first,paramId);
        inlineTemplate.argsTemplate.push_back(paramId);
    }
    emitRuntimeFunction(inlineExpr->inlineFuncBlock,genContext,this,inlineTemplate);
    emittedInlineRuntimeFuncs.insert(inlineExpr->inlineFuncRuntimeName);
}

void CodeGen::ensureInlineExprTemplates(ASTExpr *expr){
    std::vector<ASTExpr *> inlineExprs;
    collectInlineFunctionExprs(expr,inlineExprs);
    for(auto *inlineExpr : inlineExprs){
        ensureInlineFunctionTemplate(inlineExpr,"expr");
    }
}


inline void ASTIdentifier_to_RTID(ASTIdentifier *var,RTID &out){
    string_ref name = var->val;
    out.len = name.size();
    out.value = name.getBuffer();
}

static RTID makeOwnedRTID(string_ref value){
    RTID id;
    id.len = value.size();
    char *buf = new char[id.len];
    std::memcpy(buf,value.data(),id.len);
    id.value = buf;
    return id;
}

static std::string mangleClassMethodName(string_ref className,string_ref methodName){
    Twine out;
    out + "__";
    out + className.str();
    out + "__";
    out + methodName.str();
    return out.str();
}

static std::string classCtorTemplateName(size_t arity){
    Twine out;
    out + "__ctor__";
    out + std::to_string(arity);
    return out.str();
}

static std::string mangleClassCtorName(string_ref className,size_t arity){
    return mangleClassMethodName(className,classCtorTemplateName(arity));
}

static std::string mangleClassFieldInitName(string_ref className){
    Twine out;
    out + "__";
    out + className.str();
    out + "__field_init";
    return out.str();
}

static void writeNamedVarRef(std::ostream &out,string_ref name){
    RTCode code = CODE_RTVAR_REF;
    out.write((const char *)&code,sizeof(RTCode));
    RTID id = makeOwnedRTID(name);
    out << &id;
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

static bool unaryOpCodeFromSymbol(const std::string &op,RTCode &out){
    if(op == "+"){
        out = UNARY_OP_PLUS;
        return true;
    }
    if(op == "-"){
        out = UNARY_OP_MINUS;
        return true;
    }
    if(op == "!"){
        out = UNARY_OP_NOT;
        return true;
    }
    if(op == "~"){
        out = UNARY_OP_BITWISE_NOT;
        return true;
    }
    if(op == "await"){
        out = UNARY_OP_AWAIT;
        return true;
    }
    return false;
}

static bool binaryOpCodeFromSymbol(const std::string &op,RTCode &out){
    if(op == "+"){
        out = BINARY_OP_PLUS;
        return true;
    }
    if(op == "-"){
        out = BINARY_OP_MINUS;
        return true;
    }
    if(op == "*"){
        out = BINARY_OP_MUL;
        return true;
    }
    if(op == "/"){
        out = BINARY_OP_DIV;
        return true;
    }
    if(op == "%"){
        out = BINARY_OP_MOD;
        return true;
    }
    if(op == "=="){
        out = BINARY_EQUAL_EQUAL;
        return true;
    }
    if(op == "!="){
        out = BINARY_NOT_EQUAL;
        return true;
    }
    if(op == "<"){
        out = BINARY_LESS;
        return true;
    }
    if(op == "<="){
        out = BINARY_LESS_EQUAL;
        return true;
    }
    if(op == ">"){
        out = BINARY_GREATER;
        return true;
    }
    if(op == ">="){
        out = BINARY_GREATER_EQUAL;
        return true;
    }
    if(op == "&&"){
        out = BINARY_LOGIC_AND;
        return true;
    }
    if(op == "||"){
        out = BINARY_LOGIC_OR;
        return true;
    }
    if(op == "&"){
        out = BINARY_BITWISE_AND;
        return true;
    }
    if(op == "|"){
        out = BINARY_BITWISE_OR;
        return true;
    }
    if(op == "^"){
        out = BINARY_BITWISE_XOR;
        return true;
    }
    if(op == "<<"){
        out = BINARY_SHIFT_LEFT;
        return true;
    }
    if(op == ">>"){
        out = BINARY_SHIFT_RIGHT;
        return true;
    }
    return false;
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
            if(spec.expr){
                if(spec.expr->type == INLINE_FUNC_EXPR){
                    auto *inlineExpr = (ASTExpr *)spec.expr;
                    ensureInlineFunctionTemplate(inlineExpr,var_id ? var_id->val : "var");
                }
                else {
                    ensureInlineExprTemplates(spec.expr);
                }
            }

            RTVar code_var;
            ASTIdentifier_to_RTID(var_id,code_var.id);
            code_var.hasInitValue = (spec.expr != nullptr);
            code_var.attributes = convertAttributes(varDecl->attributes);
            genContext->out << &code_var;
            if(spec.expr) {
                ASTExpr *initialVal = spec.expr;
                if(initialVal->type == INLINE_FUNC_EXPR){
                    ensureInlineFunctionTemplate(initialVal,var_id ? var_id->val : "var");
                    RTCode code = CODE_RTFUNC_REF;
                    genContext->out.write((const char *)&code,sizeof(RTCode));
                    RTID funcId = makeOwnedRTID(initialVal->inlineFuncRuntimeName);
                    genContext->out << &funcId;
                }
                else {
                    consumeStmt(initialVal);
                }
            }
        };
    }
    else if(stmt->type == SECURE_DECL){
        auto *secureDecl = (ASTSecureDecl *)stmt;
        if(!secureDecl->guardedDecl || secureDecl->guardedDecl->specs.empty() || !secureDecl->catchBlock){
            return;
        }
        auto &guardedSpec = secureDecl->guardedDecl->specs.front();
        if(!guardedSpec.id || !guardedSpec.expr){
            return;
        }
        ensureInlineExprTemplates(guardedSpec.expr);

        RTCode code = CODE_RTSECURE_DECL;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        RTID targetVarId;
        ASTIdentifier_to_RTID(guardedSpec.id,targetVarId);
        genContext->out << &targetVarId;

        bool hasCatchBinding = secureDecl->catchErrorId != nullptr;
        genContext->out.write((const char *)&hasCatchBinding,sizeof(hasCatchBinding));
        if(hasCatchBinding){
            RTID catchBindingId;
            ASTIdentifier_to_RTID(secureDecl->catchErrorId,catchBindingId);
            genContext->out << &catchBindingId;
        }

        bool hasCatchType = secureDecl->catchErrorType != nullptr;
        genContext->out.write((const char *)&hasCatchType,sizeof(hasCatchType));
        if(hasCatchType){
            RTID catchTypeId = makeOwnedRTID(std::string(secureDecl->catchErrorType->getName()));
            genContext->out << &catchTypeId;
        }

        consumeStmt(guardedSpec.expr);
        write_ASTBlockStmt_to_context(secureDecl->catchBlock,genContext,this);
    }
    else if(stmt->type == FUNC_DECL){
        ASTFuncDecl *func_node = (ASTFuncDecl *)stmt;
        RTFuncTemplate funcTemplate;
        ASTIdentifier *func_id = func_node->funcId;
        ASTIdentifier_to_RTID(func_id,funcTemplate.name);
        funcTemplate.attributes = convertAttributes(func_node->attributes);
        funcTemplate.isLazy = func_node->isLazy;
        auto orderedParams = orderedParamPairs(func_node->params);
        for(auto & param_pair : orderedParams){
            RTID param_id;
            ASTIdentifier_to_RTID(param_pair.first,param_id);
            funcTemplate.argsTemplate.push_back(param_id);
        };
        emitRuntimeFunction(func_node->blockStmt,genContext,this,funcTemplate);
    }
    else if(stmt->type == CLASS_DECL){
        auto class_decl = (ASTClassDecl *)stmt;
        RTClass cls;
        ASTIdentifier_to_RTID(class_decl->id,cls.name);
        cls.attributes = convertAttributes(class_decl->attributes);
        if(class_decl->superClass){
            cls.hasSuperClass = true;
            cls.superClassName = makeOwnedRTID(std::string(class_decl->superClass->getName()));
        }
        std::string className = class_decl->id->val;
        struct FieldInitSpec {
            std::string fieldName;
            ASTExpr *expr = nullptr;
        };
        std::vector<FieldInitSpec> fieldInitializers;
        for(auto &fieldDecl : class_decl->fields){
            for(auto &spec : fieldDecl->specs){
                RTVar fieldVar;
                ASTIdentifier_to_RTID(spec.id,fieldVar.id);
                fieldVar.hasInitValue = (spec.expr != nullptr);
                fieldVar.attributes = convertAttributes(fieldDecl->attributes);
                cls.fields.push_back(std::move(fieldVar));
                if(spec.expr){
                    FieldInitSpec fieldInit;
                    fieldInit.fieldName = spec.id->val;
                    fieldInit.expr = spec.expr;
                    fieldInitializers.push_back(std::move(fieldInit));
                }
            }
        }
        for(auto &methodDecl : class_decl->methods){
            RTFuncTemplate methodTemplate;
            ASTIdentifier_to_RTID(methodDecl->funcId,methodTemplate.name);
            methodTemplate.attributes = convertAttributes(methodDecl->attributes);
            methodTemplate.isLazy = methodDecl->isLazy;
            auto orderedMethodParams = orderedParamPairs(methodDecl->params);
            for(auto &paramPair : orderedMethodParams){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                methodTemplate.argsTemplate.push_back(paramId);
            }
            cls.methods.push_back(std::move(methodTemplate));
        }
        for(auto &ctorDecl : class_decl->constructors){
            RTFuncTemplate ctorTemplate;
            ctorTemplate.name = makeOwnedRTID(classCtorTemplateName(ctorDecl->params.size()));
            ctorTemplate.attributes = convertAttributes(ctorDecl->attributes);
            auto orderedCtorParams = orderedParamPairs(ctorDecl->params);
            for(auto &paramPair : orderedCtorParams){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                ctorTemplate.argsTemplate.push_back(paramId);
            }
            cls.constructors.push_back(std::move(ctorTemplate));
        }
        if(!fieldInitializers.empty()){
            cls.hasFieldInitFunc = true;
            cls.fieldInitFuncName = makeOwnedRTID(mangleClassFieldInitName(className));
        }
        genContext->out << &cls;

        for(auto &methodDecl : class_decl->methods){
            RTFuncTemplate runtimeMethod;
            runtimeMethod.name = makeOwnedRTID(mangleClassMethodName(className,methodDecl->funcId->val));
            runtimeMethod.attributes = convertAttributes(methodDecl->attributes);
            runtimeMethod.isLazy = methodDecl->isLazy;
            auto orderedMethodParams = orderedParamPairs(methodDecl->params);
            for(auto &paramPair : orderedMethodParams){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                runtimeMethod.argsTemplate.push_back(paramId);
            }
            emitRuntimeFunction(methodDecl->blockStmt,genContext,this,runtimeMethod);
        }
        for(auto &ctorDecl : class_decl->constructors){
            RTFuncTemplate runtimeCtor;
            runtimeCtor.name = makeOwnedRTID(mangleClassCtorName(className,ctorDecl->params.size()));
            runtimeCtor.attributes = convertAttributes(ctorDecl->attributes);
            auto orderedCtorParams = orderedParamPairs(ctorDecl->params);
            for(auto &paramPair : orderedCtorParams){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                runtimeCtor.argsTemplate.push_back(paramId);
            }
            emitRuntimeFunction(ctorDecl->blockStmt,genContext,this,runtimeCtor);
        }
        if(!fieldInitializers.empty()){
            RTFuncTemplate runtimeFieldInit;
            runtimeFieldInit.name = makeOwnedRTID(mangleClassFieldInitName(className));
            std::ostringstream bodyBuffer(std::ios::out | std::ios::binary);
            auto tempOutputPath = genContext->outputPath;
            ModuleGenContext tempContext(genContext->name,bodyBuffer,tempOutputPath);
            tempContext.tableContext = genContext->tableContext;
            auto *savedContext = genContext;
            setContext(&tempContext);
            RTCode code = CODE_RTMEMBER_SET;
            for(auto &fieldInit : fieldInitializers){
                ensureInlineExprTemplates(fieldInit.expr);
                tempContext.out.write((char *)&code,sizeof(RTCode));
                writeNamedVarRef(tempContext.out,"self");
                RTID memberId = makeOwnedRTID(fieldInit.fieldName);
                tempContext.out << &memberId;
                consumeStmt(fieldInit.expr);
            }
            setContext(savedContext);
            auto bodyBytes = bodyBuffer.str();
            runtimeFieldInit.blockByteSize = bodyBytes.size();
            genContext->out << &runtimeFieldInit;
            code = CODE_RTFUNCBLOCK_BEGIN;
            genContext->out.write((char *)&code,sizeof(RTCode));
            if(!bodyBytes.empty()){
                genContext->out.write(bodyBytes.data(),(std::streamsize)bodyBytes.size());
            }
            code = CODE_RTFUNCBLOCK_END;
            genContext->out.write((char *)&code,sizeof(RTCode));
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
        for(auto & cond : cond_decl->specs){
            if(!cond.isElse() && cond.expr){
                ensureInlineExprTemplates(cond.expr);
            }
        }
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
    else if(stmt->type == FOR_DECL || stmt->type == WHILE_DECL){
        ASTExpr *loopExpr = nullptr;
        ASTBlockStmt *loopBlock = nullptr;
        if(stmt->type == FOR_DECL){
            auto *forDecl = (ASTForDecl *)stmt;
            loopExpr = forDecl->expr;
            loopBlock = forDecl->blockStmt;
        }
        else {
            auto *whileDecl = (ASTWhileDecl *)stmt;
            loopExpr = whileDecl->expr;
            loopBlock = whileDecl->blockStmt;
        }
        if(!loopExpr || !loopBlock){
            return;
        }
        ensureInlineExprTemplates(loopExpr);

        RTCode code = CODE_CONDITIONAL;
        genContext->out.write((char *)&code,sizeof(RTCode));
        unsigned count = 1;
        genContext->out.write((char *)&count,sizeof(count));
        RTCode spec_ty = COND_TYPE_LOOPIF;
        genContext->out.write((char *)&spec_ty,sizeof(RTCode));
        consumeStmt(loopExpr);
        write_ASTBlockStmt_to_context(loopBlock,genContext,this);
        code = CODE_CONDITIONAL_END;
        genContext->out.write((char *)&code,sizeof(RTCode));
    }
    else if(stmt->type == RETURN_DECL){
        ASTReturnDecl *return_decl = (ASTReturnDecl *)stmt;
        bool hasValue = return_decl->expr != nullptr;
        if(hasValue){
            ensureInlineExprTemplates(return_decl->expr);
        }
        RTCode code = CODE_RTRETURN;
        genContext->out.write((char *)&code,sizeof(RTCode));
        genContext->out.write((char *)&hasValue,sizeof(hasValue));
        if(hasValue)
            consumeStmt(return_decl->expr);

    };
}

bool stmtCanBeConvertedToRTInternalObject(ASTStmt *stmt){
    return (stmt->type == ARRAY_EXPR) || (stmt->type & LITERAL);
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
    if(!stmt){
        return;
    }
    ASTExpr *expr = (ASTExpr *)stmt;
    if((stmt->type & EXPR) && exprEmitDepth == 0){
        ensureInlineExprTemplates(expr);
    }
    struct ExprDepthGuard {
        size_t &depth;
        explicit ExprDepthGuard(size_t &depthRef):depth(depthRef){ ++depth; }
        ~ExprDepthGuard(){ --depth; }
    } depthGuard(exprEmitDepth);
    if(stmt->type == REGEX_LITERAL){
        auto *literalExpr = (ASTLiteralExpr *)stmt;
        RTCode code = CODE_RTREGEX_LITERAL;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        RTID patternId = makeOwnedRTID(literalExpr->regexPattern.value_or(""));
        RTID flagsId = makeOwnedRTID(literalExpr->regexFlags.value_or(""));
        genContext->out << &patternId;
        genContext->out << &flagsId;
        return;
    }
    if(stmt->type == DICT_EXPR){
        RTCode code = CODE_RTDICT_LITERAL;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        unsigned pairCount = expr->dictExpr.size();
        genContext->out.write((const char *)&pairCount,sizeof(pairCount));
        for(auto &pair : expr->dictExpr){
            consumeStmt(pair.first);
            consumeStmt(pair.second);
        }
        return;
    }
    /// Literals
    if(stmtCanBeConvertedToRTInternalObject(expr)){
        auto obj = exprToRTInternalObject(expr);
        genContext->out << &obj;
        StarbytesObjectRelease(obj);
    }
    else if(stmt->type == INLINE_FUNC_EXPR){
        ensureInlineFunctionTemplate(expr,"expr");
        if(expr->inlineFuncRuntimeName.empty()){
            return;
        }
        RTCode code = CODE_RTFUNC_REF;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        RTID id = makeOwnedRTID(expr->inlineFuncRuntimeName);
        genContext->out << &id;
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
    else if(stmt->type == INDEX_EXPR){
        RTCode code = CODE_RTINDEX_GET;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        consumeStmt(expr->leftExpr);
        consumeStmt(expr->rightExpr);
    }
    else if(stmt->type == UNARY_EXPR){
        auto op = expr->oprtr_str.value_or("");
        RTCode unaryCode = UNARY_OP_NOT;
        if(!unaryOpCodeFromSymbol(op,unaryCode)){
            return;
        }
        RTCode code = CODE_UNARY_OPERATOR;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        genContext->out.write((const char *)&unaryCode,sizeof(unaryCode));
        consumeStmt(expr->leftExpr);
    }
    else if(stmt->type == BINARY_EXPR){
        auto op = expr->oprtr_str.value_or("");
        if(op == KW_IS){
            if(!expr->leftExpr || !expr->runtimeTypeCheckName.has_value()){
                return;
            }
            RTCode code = CODE_RTTYPECHECK;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            consumeStmt(expr->leftExpr);
            RTID typeId = makeOwnedRTID(expr->runtimeTypeCheckName.value());
            genContext->out << &typeId;
            return;
        }
        RTCode binaryCode = BINARY_OP_PLUS;
        if(!binaryOpCodeFromSymbol(op,binaryCode)){
            return;
        }
        RTCode code = CODE_BINARY_OPERATOR;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        genContext->out.write((const char *)&binaryCode,sizeof(binaryCode));
        consumeStmt(expr->leftExpr);
        consumeStmt(expr->rightExpr);
    }
    else if(stmt->type == TERNARY_EXPR){
        if(!expr->leftExpr || !expr->middleExpr || !expr->rightExpr){
            return;
        }
        RTCode code = CODE_RTTERNARY;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        consumeStmt(expr->leftExpr);
        consumeStmt(expr->middleExpr);
        consumeStmt(expr->rightExpr);
    }
    else if(stmt->type == ASSIGN_EXPR){
        if(!expr->leftExpr || !expr->rightExpr){
            return;
        }
        auto assignOp = expr->oprtr_str.value_or("=");
        bool isCompound = assignOp != "=";
        RTCode compoundBinaryCode = BINARY_OP_PLUS;
        if(isCompound){
            std::string binaryOp;
            if(assignOp == "+="){
                binaryOp = "+";
            }
            else if(assignOp == "-="){
                binaryOp = "-";
            }
            else if(assignOp == "*="){
                binaryOp = "*";
            }
            else if(assignOp == "/="){
                binaryOp = "/";
            }
            else if(assignOp == "%="){
                binaryOp = "%";
            }
            else if(assignOp == "&="){
                binaryOp = "&";
            }
            else if(assignOp == "|="){
                binaryOp = "|";
            }
            else if(assignOp == "^="){
                binaryOp = "^";
            }
            else if(assignOp == "<<="){
                binaryOp = "<<";
            }
            else if(assignOp == ">>="){
                binaryOp = ">>";
            }
            else {
                return;
            }
            if(!binaryOpCodeFromSymbol(binaryOp,compoundBinaryCode)){
                return;
            }
        }

        auto emitAssignedValueExpr = [&](){
            if(!isCompound){
                consumeStmt(expr->rightExpr);
                return;
            }
            RTCode code = CODE_BINARY_OPERATOR;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            genContext->out.write((const char *)&compoundBinaryCode,sizeof(compoundBinaryCode));
            consumeStmt(expr->leftExpr);
            consumeStmt(expr->rightExpr);
        };

        if(expr->leftExpr->type == ID_EXPR && expr->leftExpr->id && expr->leftExpr->id->type == ASTIdentifier::Var){
            RTCode code = CODE_RTVAR_SET;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            RTID varId;
            ASTIdentifier_to_RTID(expr->leftExpr->id,varId);
            genContext->out << &varId;
            emitAssignedValueExpr();
            return;
        }

        if(expr->leftExpr->type == MEMBER_EXPR){
            std::string memberName;
            if(!getMemberName(expr->leftExpr,memberName)){
                return;
            }
            RTCode code = CODE_RTMEMBER_SET;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            consumeStmt(expr->leftExpr->leftExpr);
            RTID memberId = makeOwnedRTID(memberName);
            genContext->out << &memberId;
            emitAssignedValueExpr();
            return;
        }

        if(expr->leftExpr->type == INDEX_EXPR){
            RTCode code = CODE_RTINDEX_SET;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            consumeStmt(expr->leftExpr->leftExpr);
            consumeStmt(expr->leftExpr->rightExpr);
            emitAssignedValueExpr();
            return;
        }
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
