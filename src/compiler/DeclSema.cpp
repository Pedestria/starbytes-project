#include "starbytes/compiler/SemanticA.h"
#include <sstream>
#include <memory>

namespace starbytes {

static bool shouldSuppressUnusedInvocationWarning(ASTStmt *stmt){
    if(!stmt || stmt->type != IVKE_EXPR){
        return false;
    }
    auto *expr = (ASTExpr *)stmt;
    if(!expr->callee){
        return false;
    }
    if(expr->callee->type == ID_EXPR && expr->callee->id){
        return expr->callee->id->val == "print";
    }
    if(expr->callee->type == MEMBER_EXPR && expr->callee->rightExpr && expr->callee->rightExpr->id){
        return expr->callee->rightExpr->id->val == "print";
    }
    return false;
}

static void emitConstantConditionDiagnostic(DiagnosticHandler &errStream,
                                            ASTStmt *diagNode,
                                            const std::string &message,
                                            const std::string &reason = ""){
    std::ostringstream ss;
    ss << message;
    if(!reason.empty()){
        ss << " Reason: " << reason << ".";
    }
    errStream.push(SemanticADiagnostic::create(ss.str(),diagNode,Diagnostic::Warning));
}

ASTType * SemanticA::evalGenericDecl(ASTDecl *stmt,
                                     Semantics::STableContext & symbolTableContext,
                                     ASTScopeSemanticsContext & scopeContext,
                                     bool * hasErrored,
                                     ASTType *expectedReturnType){
    ASTType *t = nullptr;
    switch (stmt->type) {
        /// VarDecl
        case VAR_DECL : {
            auto varDecl = (ASTVarDecl *)stmt;
            bool isNativeVarDecl = false;
            for(const auto &attr : varDecl->attributes){
                if(attr.name == "native"){
                    isNativeVarDecl = true;
                    break;
                }
            }
            for(auto & spec : varDecl->specs){
                if(varDecl->isConst && !spec.expr && !isNativeVarDecl){
                    std::ostringstream ss;
                    ss << "Const variable `" << spec.id->val << "` must be initialized.";
                    errStream.push(SemanticADiagnostic::create(ss.str(),varDecl,Diagnostic::Error));
                    *hasErrored = true;
                    return nullptr;
                }
                if(spec.type != nullptr){
                    if(!typeExists(spec.type,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,varDecl)){
                        *hasErrored = true;
                        return nullptr;
                    }
                    if(spec.expr){
                        if(varDecl->isSecureWrapped){
                            auto *exprType = evalExprForTypeId(spec.expr,symbolTableContext,scopeContext);
                            if(!exprType){
                                *hasErrored = true;
                                return nullptr;
                            }
                            ASTType *checkType = exprType;
                            if(exprType->isOptional || exprType->isThrowable){
                                checkType = ASTType::Create(exprType->getName(),varDecl,false,false);
                            }
                            if(!spec.type->match(checkType,[&](std::string message){
                                std::ostringstream ss;
                                ss << message << "\nContext: Type `" << exprType->getName() << "` was implied from secure var initializer";
                                errStream.push(SemanticADiagnostic::create(ss.str(),varDecl,Diagnostic::Error));
                            })){
                                *hasErrored = true;
                                return nullptr;
                            }
                        }
                        else {
                            // Type Decl and Type Implication Comparison
                            if(!typeMatches(spec.type,spec.expr,symbolTableContext,scopeContext))
                            {
                                *hasErrored = true;
                                return nullptr;
                            }
                        }
                    }
                }
                else {
                    if(!spec.expr){
                        /// No Type Implication nor any type decl
                        auto id = spec.id;
                        std::ostringstream ss;
                        ss << "Variable `" << id->val << "` has no type assignment nor an initial value thus the variable's type cannot be deduced.";
                        auto res = ss.str();
                        errStream.push(SemanticADiagnostic::create(res,varDecl,Diagnostic::Error));
                        *hasErrored = true;
                        return nullptr;
                    }
                    /// Type implication only.
                    auto type = evalExprForTypeId(spec.expr,symbolTableContext,scopeContext);
                    if(!type){
                        *hasErrored = true;
                        return nullptr;
                    };
                    if(varDecl->isSecureWrapped && (type->isOptional || type->isThrowable)){
                        auto *normalizedType = ASTType::Create(type->getName(),varDecl,false,false);
                        spec.type = normalizedType;
                    }
                    else {
                    spec.type = type;
                    if(spec.expr && (type->nameMatches(LONG_TYPE) || type->nameMatches(DOUBLE_TYPE))){
                        spec.expr->runtimeCastTargetName = type->getName().str();
                    }
                    }
                }

                if(spec.expr){
                    auto *initType = evalExprForTypeId(spec.expr,symbolTableContext,scopeContext);
                    if(!initType){
                        *hasErrored = true;
                        return nullptr;
                    }
                    if((initType->isOptional || initType->isThrowable) && !varDecl->isSecureWrapped){
                        errStream.push(SemanticADiagnostic::create("Optional or throwable values must be captured with a secure declaration.",varDecl,Diagnostic::Error));
                        *hasErrored = true;
                        return nullptr;
                    }
                }
            };
            break;
        }
        case COND_DECL:
        {
            auto *condDecl = (ASTConditionalDecl *)stmt;
            for(size_t specIndex = 0;specIndex < condDecl->specs.size();++specIndex){
                auto &condSpec = condDecl->specs[specIndex];
                if(!condSpec.isElse()){
                    ASTExpr *conditionExpr = condSpec.expr;
                    ASTType *condResType = evalExprForTypeId(conditionExpr,symbolTableContext,scopeContext);
                    /// 1. Check if expression evaluation failed.
                    if(!condResType){
                        *hasErrored = true;
                        return nullptr;
                    };
                    
                    /// 2. Check if expression return type is a Bool
                    if(!condResType->match(BOOL_TYPE,[&](std::string message){
                        errStream.push(SemanticADiagnostic::create("Context: Expression was evaluated in as a conditional specifier",conditionExpr,Diagnostic::Error));
                    })
                    ){
                        *hasErrored = true;
                        return nullptr;
                    };
                    if(auto constantInfo = evaluateCompileTimeBoolExpr(conditionExpr,symbolTableContext,scopeContext)){
                        if(constantInfo->value){
                            if(specIndex + 1 < condDecl->specs.size()){
                                emitConstantConditionDiagnostic(errStream,
                                                                conditionExpr,
                                                                "Conditional condition is always true; subsequent branches are unreachable.",
                                                                constantInfo->reason);
                            }
                            else {
                                emitConstantConditionDiagnostic(errStream,
                                                                conditionExpr,
                                                                "Conditional condition is always true; branch body always executes.",
                                                                constantInfo->reason);
                            }
                        }
                        else {
                            emitConstantConditionDiagnostic(errStream,
                                                            conditionExpr,
                                                            "Conditional condition is always false; branch body is unreachable.",
                                                            constantInfo->reason);
                        }
                    }
                    
                }
               /// 3. Eval Block
                bool hasFailed = false;
                ASTType *blockReturnType = evalBlockStmtForASTType(condSpec.blockStmt,
                                                                   symbolTableContext,
                                                                   &hasFailed,
                                                                   scopeContext,
                                                                   scopeContext.args != nullptr,
                                                                   expectedReturnType);
                if(hasFailed){
                    *hasErrored = hasFailed;
                    return nullptr;
                };
                
                if(blockReturnType)
                    t = blockReturnType;
                
            };
            
            
            break;
        }
        case FOR_DECL:
        case WHILE_DECL:
        {
            ASTExpr *conditionExpr = nullptr;
            ASTBlockStmt *loopBlock = nullptr;
            if(stmt->type == FOR_DECL){
                auto *forDecl = (ASTForDecl *)stmt;
                conditionExpr = forDecl->expr;
                loopBlock = forDecl->blockStmt;
            }
            else {
                auto *whileDecl = (ASTWhileDecl *)stmt;
                conditionExpr = whileDecl->expr;
                loopBlock = whileDecl->blockStmt;
            }

            if(!conditionExpr || !loopBlock){
                errStream.push(SemanticADiagnostic::create("Malformed loop declaration.",stmt,Diagnostic::Error));
                *hasErrored = true;
                return nullptr;
            }

            ASTType *condResType = evalExprForTypeId(conditionExpr,symbolTableContext,scopeContext);
            if(!condResType){
                *hasErrored = true;
                return nullptr;
            }

            if(!condResType->match(BOOL_TYPE,[&](std::string){
                errStream.push(SemanticADiagnostic::create("Context: Expression was evaluated in as a loop condition",conditionExpr,Diagnostic::Error));
            })){
                *hasErrored = true;
                return nullptr;
            }
            if(auto constantInfo = evaluateCompileTimeBoolExpr(conditionExpr,symbolTableContext,scopeContext)){
                if(constantInfo->value){
                    emitConstantConditionDiagnostic(errStream,
                                                    conditionExpr,
                                                    "Loop condition is always true; loop may be infinite unless exited internally.",
                                                    constantInfo->reason);
                }
                else {
                    emitConstantConditionDiagnostic(errStream,
                                                    conditionExpr,
                                                    "Loop condition is always false; loop body is unreachable.",
                                                    constantInfo->reason);
                }
            }

            bool hasFailed = false;
            ASTType *blockReturnType = evalBlockStmtForASTType(loopBlock,
                                                               symbolTableContext,
                                                               &hasFailed,
                                                               scopeContext,
                                                               scopeContext.args != nullptr,
                                                               expectedReturnType);
            if(hasFailed){
                *hasErrored = true;
                return nullptr;
            }

            if(blockReturnType){
                t = blockReturnType;
            }
            break;
        }
        case SECURE_DECL:
        {
            auto *secureDecl = (ASTSecureDecl *)stmt;
            if(!secureDecl->guardedDecl || secureDecl->guardedDecl->specs.empty() || !secureDecl->catchBlock){
                errStream.push(SemanticADiagnostic::create("Malformed secure declaration.",stmt,Diagnostic::Error));
                *hasErrored = true;
                return nullptr;
            }

            bool guardedDeclErrored = false;
            evalGenericDecl(secureDecl->guardedDecl,symbolTableContext,scopeContext,&guardedDeclErrored,expectedReturnType);
            if(guardedDeclErrored){
                *hasErrored = true;
                return nullptr;
            }

            auto &guardedSpec = secureDecl->guardedDecl->specs.front();
            if(!guardedSpec.expr){
                errStream.push(SemanticADiagnostic::create("Secure declaration requires an initializer expression.",stmt,Diagnostic::Error));
                *hasErrored = true;
                return nullptr;
            }

            auto *guardedExprType = evalExprForTypeId(guardedSpec.expr,symbolTableContext,scopeContext);
            if(!guardedExprType){
                *hasErrored = true;
                return nullptr;
            }
            if(!guardedExprType->isOptional && !guardedExprType->isThrowable){
                errStream.push(SemanticADiagnostic::create("Secure declaration requires an optional or throwable initializer.",stmt,Diagnostic::Error));
                *hasErrored = true;
                return nullptr;
            }

            if(secureDecl->catchErrorType && !typeExists(secureDecl->catchErrorType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,secureDecl)){
                errStream.push(SemanticADiagnostic::create("Unknown catch error type in secure declaration.",stmt,Diagnostic::Error));
                *hasErrored = true;
                return nullptr;
            }

            ASTScopeSemanticsContext catchScopeContext {secureDecl->catchBlock->parentScope,
                                                        scopeContext.args,
                                                        scopeContext.genericTypeParams,
                                                        scopeContext.scopedBindings};
            if(secureDecl->catchErrorId){
                catchScopeContext.scopedBindings.push_back({
                    secureDecl->catchBlock->parentScope,
                    secureDecl->catchErrorId,
                    secureDecl->catchErrorType ? secureDecl->catchErrorType : STRING_TYPE
                });
            }

            bool catchBlockFailed = false;
            ASTType *catchBlockReturnType = evalBlockStmtForASTType(secureDecl->catchBlock,
                                                                    symbolTableContext,
                                                                    &catchBlockFailed,
                                                                    catchScopeContext,
                                                                    catchScopeContext.args != nullptr,
                                                                    expectedReturnType);
            if(catchBlockFailed){
                *hasErrored = true;
                return nullptr;
            }

            if(catchBlockReturnType){
                t = catchBlockReturnType;
            }
            break;
        }
        default: {
            *hasErrored = true;
            return nullptr;
            break;
        }
    }
    *hasErrored = false;
    return t;
}

ASTType *SemanticA::evalBlockStmtForASTType(ASTBlockStmt *stmt,
                                            Semantics::STableContext & symbolTableContext,
                                            bool  * hasErrored,
                                            ASTScopeSemanticsContext & scopeContext,
                                            bool inFuncContext,
                                            ASTType *expectedReturnType){
        ASTType *returnType = VOID_TYPE;
        ASTScopeSemanticsContext blockScopeContext = scopeContext;
        if(stmt && stmt->parentScope){
            blockScopeContext.scope = stmt->parentScope;
        }

        auto blockSymbols = std::make_shared<Semantics::SymbolTable>();
        symbolTableContext.otherTables.push_back(blockSymbols);
        struct ScopedBlockSymbolCleanup {
            Semantics::STableContext &tableContext;
            ~ScopedBlockSymbolCleanup(){
                if(!tableContext.otherTables.empty()){
                    tableContext.otherTables.pop_back();
                }
            }
        } scopedBlockSymbolCleanup {symbolTableContext};

        #define RETURN() if(inFuncContext)\
            return returnType;\
            else\
            return nullptr;
        ASTType *mainReturnType = nullptr;
        bool unreachableCodeAfterReturn = false;
        std::vector<ASTType *> returnTypes;
        
        for (auto & node : stmt->body) {
            if(mainReturnType != nullptr && !unreachableCodeAfterReturn){
                errStream.push(SemanticADiagnostic::create("The code here and below is unreachable.",node,Diagnostic::Warning));
                unreachableCodeAfterReturn = true;
                continue;
            }
            
            if(node->type & DECL){
                if(node->type == RETURN_DECL){
                    if(inFuncContext){
                        auto *ret = (ASTReturnDecl *)node;
                        /// If ReturnDecl has no value return..
                        /// Like this -->
                        ///
                        /// func test(){
                        ///   // Do Stuff
                        ///   return
                        /// }
                        /// // No Warning Issued due to implied return type being Void.
                        /// test()
                        if(!ret->expr){
                            mainReturnType = VOID_TYPE;
                            continue;
                        };
                        if(expectedReturnType){
                            if(!typeMatches(expectedReturnType,ret->expr,symbolTableContext,blockScopeContext,"return value")){
                                *hasErrored = true;
                                return nullptr;
                            }
                            mainReturnType = expectedReturnType;
                        }
                        else {
                            auto rt = evalExprForTypeId(ret->expr,symbolTableContext,blockScopeContext);
                            if(!rt){
                                *hasErrored = true;
                                return nullptr;
                            };
                            mainReturnType = rt;
                        }
                    }
                    else {
                        errStream.push(SemanticADiagnostic::create("Cannot declare a return outside of a func scope.",node,Diagnostic::Error));
                        *hasErrored = true;
                        return nullptr;
                    }
                    
                }
                else {;
                    auto declNode = (ASTDecl *)node;
                    ASTType *declReturn = nullptr;
                    bool requiresStatementLevelValidation = false;
                    switch(declNode->type){
                        case FUNC_DECL:
                        case CLASS_DECL:
                        case INTERFACE_DECL:
                        case TYPE_ALIAS_DECL:
                        case IMPORT_DECL:
                        case SCOPE_DECL:
                            requiresStatementLevelValidation = true;
                            break;
                        default:
                            break;
                    }

                    if(requiresStatementLevelValidation){
                        if(!checkSymbolsForStmtInScope(declNode,symbolTableContext,blockScopeContext.scope)){
                            *hasErrored = true;
                            return nullptr;
                        }
                    }
                    else {
                        bool _hasErrored = false;
                        declReturn = evalGenericDecl(declNode,
                                                     symbolTableContext,
                                                     blockScopeContext,
                                                     &_hasErrored,
                                                     expectedReturnType);
                        if(_hasErrored)
                        {
                            *hasErrored = _hasErrored;
                            return nullptr;
                        }
                    }
                    addSTableEntryForDecl(declNode,blockSymbols.get());
                    if(inFuncContext){
                        returnTypes.push_back(declReturn);
                    };
                }
            }
            else{
                ASTType *t = evalExprForTypeId((ASTExpr *)node,symbolTableContext,blockScopeContext);
                if(!t){
                    *hasErrored = true;
                    return nullptr;
                };
                
                if(node->type == IVKE_EXPR && t != VOID_TYPE && !shouldSuppressUnusedInvocationWarning(node)){
                    
                        auto *ivk = (ASTType *)node;
                        std::ostringstream ss;
                        ss <<  "Invocation returns a value but is not stored.";
                        auto out = ss.str();
                        errStream.push(SemanticADiagnostic::create(out,node,Diagnostic::Warning));
                        /// Warn of non void object not being stored after creation from function invocation.
                };
            };
        }
        /// If there is no main return type.
        if(!mainReturnType){
            if(expectedReturnType){
                for(auto & retType : returnTypes){
                    if(retType && !retType->nameMatches(VOID_TYPE)){
                        returnType = expectedReturnType;
                        break;
                    }
                };
            }
            else {
                for(auto & retType : returnTypes){
                    if(retType && !retType->nameMatches(VOID_TYPE)){
                        returnType = retType;
                        break;
                    }
                };
            }
        }
        else {
            returnType = mainReturnType;
        };
        
        
        RETURN()
        
        #undef RETURN
    }


}
