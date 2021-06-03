#include "starbytes/Parser/SemanticA.h"

namespace starbytes {

ASTType * SemanticA::evalGenericDecl(ASTDecl *stmt,Semantics::STableContext & symbolTableContext,ASTScopeSemanticsContext & scopeContext,bool * hasErrored){
    ASTType *t = nullptr;
    switch (stmt->type) {
        /// VarDecl
        case VAR_DECL : {
            auto varDecl = (ASTVarDecl *)stmt;
            for(auto & spec : varDecl->specs){
                if(!(!spec.type) && (!spec.expr)){
                    // Type Decl and Type Implication Comparsion
                    if(!typeMatches(spec.type,spec.expr,symbolTableContext,scopeContext))
                    {
                        *hasErrored = true;
                        return nullptr;
                    }
                }
                else if((!spec.type) && (!spec.expr)) {
                    /// No Type Implication nor any type decl
                    auto id = spec.id;
                    errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Variable `{0}` has no type assignment nor an initial value thus the variable's type cannot be deduced.",id->val),varDecl);
                    *hasErrored = true;
                    return nullptr;
                }
                else {
                    /// Type Implication Only.
                    auto type = evalExprForTypeId(spec.expr,symbolTableContext,scopeContext);
                    if(!type){
                        *hasErrored = true;
                        return nullptr;
                    };
                    spec.type = type;
                };
            };
            break;
        }
        case COND_DECL:
        {
            ASTConditionalDecl *condDecl = (ASTConditionalDecl *)stmt;
            for(auto & condSpec : condDecl->specs){
                if(!condSpec.isElse()){
                    ASTExpr *conditionExpr = condSpec.expr;
                    ASTType *condResType = evalExprForTypeId(conditionExpr,symbolTableContext,scopeContext);
                    /// 1. Check if expression evaluation failed.
                    if(!condResType){
                        *hasErrored = true;
                        return nullptr;
                    };
                    
                    /// 2. Check if expression return type is a Bool
                    if(!condResType->match(BOOL_TYPE,[&](const llvm::formatv_object_base & message){
                        errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("{0}\nContext: Expression was evaluated in as a conditional specifier"),conditionExpr);
                    })){
                        *hasErrored = true;
                        return nullptr;
                    };
                    
                }
               /// 3. Eval Block
                bool hasFailed;
                ASTType *blockReturnType = evalBlockStmtForASTType(condSpec.blockStmt, symbolTableContext,&hasFailed,scopeContext,scopeContext.args != nullptr);
                if(hasFailed){
                    *hasErrored = hasFailed;
                    return nullptr;
                };
                
                if(scopeContext.scope->type == ASTScope::Function)
                    t = blockReturnType;
                
            };
            
            
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

ASTType *SemanticA::evalBlockStmtForASTType(ASTBlockStmt *stmt,Semantics::STableContext & symbolTableContext,bool  * hasErrored,ASTScopeSemanticsContext & scopeContext,bool inFuncContext){
        ASTType *returnType = VOID_TYPE;

        #define RETURN() if(stmt->parentScope->type == ASTScope::Function)\
            return returnType;\
            else\
            return nullptr;
        ASTType *mainReturnType = nullptr;
        bool unreachableCodeAfterReturn = false;
        llvm::SmallVector<ASTType *,2> returnTypes;
        
        for (auto & node : stmt->body) {
            if(mainReturnType != nullptr && !unreachableCodeAfterReturn){
                errStream << new SemanticADiagnostic(SemanticADiagnostic::Warning,llvm::formatv("The code here and below is unreachable."),node);
                unreachableCodeAfterReturn = true;
                continue;
            }
            
            if(node->type & DECL){
                if(node->type == RETURN_DECL){
                    if(inFuncContext){
                        ASTReturnDecl *ret = (ASTReturnDecl *)node;
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
                        auto rt = evalExprForTypeId(ret->expr,symbolTableContext,scopeContext);
                        if(!rt){
                            *hasErrored = true;
                            return nullptr;
                        };
                        mainReturnType = rt;
                    }
                    else {
                        errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Cannot declare a  return outside of a func scope."),node);
                        *hasErrored = true;
                        return nullptr;
                    }
                    
                }
                else {;
                    
                    bool __hasErrored;
                    auto declReturn = evalGenericDecl((ASTDecl *)node,symbolTableContext,scopeContext,&__hasErrored);
                    if(__hasErrored)
                    {
                        *hasErrored = __hasErrored;
                        return nullptr;
                    }
                    if(inFuncContext){
                        returnTypes.push_back(declReturn);
                    };
                }
            }
            else{
                ASTType *t = evalExprForTypeId((ASTExpr *)node,symbolTableContext,scopeContext);
                if(!t){
                    *hasErrored = true;
                    return nullptr;
                };
                
                if(node->type == IVKE_EXPR){
                    if(t != VOID_TYPE){
                        ASTFuncDecl *funcNode = (ASTFuncDecl *)node;
                        errStream << new SemanticADiagnostic(SemanticADiagnostic::Warning,llvm::formatv("Func `{0}` returns a value but is not being stored by a variable.",funcNode->funcId->val),node);
                        /// Warn of non void object not being stored after creation from function invocation.
                    };
                };
            };
        }
        /// If there is no main return type.
        if(!returnType){
            for(auto & retType : returnTypes){
                if(!retType->nameMatches(VOID_TYPE)){
                    break;
                };
            };
        }
        else {
            
        };
        
        
        RETURN()
        
        #undef RETURN
    }


}