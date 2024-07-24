#include "starbytes/compiler/SemanticA.h"
#include <sstream>

namespace starbytes {

ASTType * SemanticA::evalGenericDecl(ASTDecl *stmt,
                                     Semantics::STableContext & symbolTableContext,
                                     ASTScopeSemanticsContext & scopeContext,
                                     bool * hasErrored){
    ASTType *t = nullptr;
    switch (stmt->type) {
        /// VarDecl
        case VAR_DECL : {
            auto varDecl = (ASTVarDecl *)stmt;
            for(auto & spec : varDecl->specs){
                if(spec.type != nullptr && (!spec.expr)){
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
                    std::ostringstream ss;
                    ss << "Variable `" << id->val << "` has no type assignment nor an initial value thus the variable's type cannot be deduced.";
                    auto res = ss.str();
                    errStream.push(SemanticADiagnostic::create(res,varDecl,Diagnostic::Error));
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
            auto *condDecl = (ASTConditionalDecl *)stmt;
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
                    if(!condResType->match(BOOL_TYPE,[&](std::string message){
                        errStream.push(SemanticADiagnostic::create("Context: Expression was evaluated in as a conditional specifier",conditionExpr,Diagnostic::Error));
                    })
                    ){
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

ASTType *SemanticA::evalBlockStmtForASTType(ASTBlockStmt *stmt,
                                            Semantics::STableContext & symbolTableContext,
                                            bool  * hasErrored,
                                            ASTScopeSemanticsContext & scopeContext,
                                            bool inFuncContext){
        ASTType *returnType = VOID_TYPE;

        #define RETURN() if(stmt->parentScope->type == ASTScope::Function)\
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
                        auto rt = evalExprForTypeId(ret->expr,symbolTableContext,scopeContext);
                        if(!rt){
                            *hasErrored = true;
                            return nullptr;
                        };
                        mainReturnType = rt;
                    }
                    else {
                        errStream.push(SemanticADiagnostic::create("Cannot declare a return outside of a func scope.",node,Diagnostic::Error));
                        *hasErrored = true;
                        return nullptr;
                    }
                    
                }
                else {;
                    
                    bool _hasErrored;
                    auto declReturn = evalGenericDecl((ASTDecl *)node,symbolTableContext,scopeContext,&_hasErrored);
                    if(_hasErrored)
                    {
                        *hasErrored = _hasErrored;
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
                        auto *funcNode = (ASTFuncDecl *)node;
                        std::ostringstream ss;
                        ss << "Func `" << funcNode->funcId->val << "` returns a value but is not being stored by a variable.";
                        auto out = ss.str();
                        errStream.push(SemanticADiagnostic::create(out,node,Diagnostic::Warning));
                        /// Warn of non void object not being stored after creation from function invocation.
                    };
                };
            };
        }
        /// If there is no main return type.
        if(!mainReturnType){
            for(auto & retType : returnTypes){
                if(!retType->nameMatches(VOID_TYPE)){
                    break;
                };
            };
        }
        else {
            returnType = mainReturnType;
        };
        
        
        RETURN()
        
        #undef RETURN
    }


}