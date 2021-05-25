#include "starbytes/Parser/SemanticA.h"
#include "starbytes/Parser/SymTable.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTNodes.def"
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/FormatVariadic.h>
#include <iostream>
namespace starbytes {

    SemanticA::SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticBufferedLogger & errStream):syntaxARef(syntaxARef),errStream(errStream){

    };

    void SemanticA::start(){
        std::cout << "Starting SemanticA" << std::endl;
    };

    void SemanticA::finish(){
        
    };
     /// Only registers new symbols associated with top level decls!
    void SemanticA::addSTableEntryForDecl(ASTDecl *decl,Semantics::SymbolTable *tablePtr){
       
        switch (decl->type) {
            case VAR_DECL : {
                ASTVarDecl *varDecl = (ASTVarDecl *)decl;
                for(auto spec : varDecl->specs) {
                    Semantics::SymbolTable::Entry *e = new Semantics::SymbolTable::Entry();
                    auto data = new Semantics::SymbolTable::Var();
                    data->type = spec.type;
                    auto * id = spec.id;
                    e->name = id->val;
                    e->data = data;
                    e->type = Semantics::SymbolTable::Entry::Var;
                    tablePtr->addSymbolInScope(e,decl->scope);
                }
                break;
            }
            case FUNC_DECL : {
                ASTFuncDecl *funcDecl = (ASTFuncDecl *)decl;
                Semantics::SymbolTable::Entry *e = new Semantics::SymbolTable::Entry();
                auto data = new Semantics::SymbolTable::Function();
                data->returnType = funcDecl->returnType;
                
                for(auto & param_pair : funcDecl->params){
                    data->paramMap.insert(std::make_pair(param_pair.getFirst()->val,param_pair.getSecond()));
                };
                
                e->name = funcDecl->funcId->val;
                e->data = data;
                e->type = Semantics::SymbolTable::Entry::Function;
                tablePtr->addSymbolInScope(e,decl->scope);
                break;
            }
            // case CLS_DECL : {
            //     ASTIdentifier *cls_id = (ASTIdentifier *)decl->declProps[0].dataPtr;
            //     Semantics::SymbolTable::Entry *e = new Semantics::SymbolTable::Entry();
            //     e->name = cls_id->val;
            //     e->node = decl;
            //     e->type = Semantics::SymbolTable::Entry::Class;
            //     tablePtr->addSymbolInScope(e,decl->scope);
            //     break;
            // }
        default : {
            break;
        }
        }
    };


#define PRINT_FUNC_ID "print"

    bool SemanticA::typeExists(ASTType *type,Semantics::STableContext &contextTableContxt){
        /// First check to see if ASTType is one of the built in types
        if(type == VOID_TYPE || type == STRING_TYPE || type == ARRAY_TYPE || 
        type == DICTIONARY_TYPE || type == BOOL_TYPE || type == INT_TYPE){
            return true;
        }
        else {
            return false;
        }
    };

    ASTType * SemanticA::evalExprForTypeId(ASTExpr *expr_to_eval, Semantics::STableContext & symbolTableContext){
        ASTType *type;
        switch (expr_to_eval->type) {
            case ID_EXPR : {
                ASTIdentifier *id_ = expr_to_eval->id;
                SemanticsContext ctxt {errStream,expr_to_eval};
                
                auto symbol_ = symbolTableContext.findEntry(id_->val,ctxt);
                if(!symbol_){
                    return nullptr;
                    break;
                };
                
                if(symbol_->type == Semantics::SymbolTable::Entry::Var){
                    auto varData = (Semantics::SymbolTable::Var *)symbol_->data;
                    type = varData->type;
                }
                else {
                    errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Identifier in this context cannot identify any other symbol type except another variable."),expr_to_eval);
                    return nullptr;
                };
                
                break;
            }
            case ARRAY_EXPR : {
                /// TODO: Check for specific Array Type
                type = ARRAY_TYPE;
                break;
            }
                /// Literals
            case BOOL_LITERAL : {
                type = BOOL_TYPE;
                break;
            }
            case STR_LITERAL:
            {
                type = STRING_TYPE;
                break;
            }
            case IVKE_EXPR : {
               
                
                /// 2. Check return type.
                ASTIdentifier *func_id = expr_to_eval->id;
                /// a. Check internal function return types.
                llvm::StringRef func_name = func_id->val;
                if(func_name == PRINT_FUNC_ID){
                    if(expr_to_eval->exprArrayData.size() > 1){
                        errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Incorrect number of arguments"),expr_to_eval);
                    }
                    /// print() -> Void
                    type = VOID_TYPE;

                    for(auto & arg : expr_to_eval->exprArrayData){
                        ASTType *_id = evalExprForTypeId(arg,symbolTableContext);
                        if(!_id){
                            return nullptr;
                            break;
                        };
                        
                    };
                }
                else {
                    /// Check to see if function was previously defined.
                    SemanticsContext ctxt {errStream,expr_to_eval};
                    auto entry = symbolTableContext.findEntry(func_name,ctxt);
                    if(!entry){
                        return nullptr;
                        break;
                    };
                    /// Check to see if symbol is function.
                    if(entry->type != Semantics::SymbolTable::Entry::Function){
                        errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Identifier `{0}` does not identify a function",entry->name),expr_to_eval);
                        return nullptr;
                        break;
                    };

                    auto funcData = (Semantics::SymbolTable::Function *)entry->data;
                    
                    if(expr_to_eval->exprArrayData.size() != funcData->paramMap.size()){
                        errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Incorrect number of arguments. Expected {0} args, but got {1}\nContext: Invocation of func `{2}`",funcData->paramMap.size(),expr_to_eval->exprArrayData.size(),func_id->val),expr_to_eval);
                        return nullptr;
                        break;
                    }
                    
                    auto param_decls_it = funcData->paramMap.begin();
                    for(unsigned i = 0;i < expr_to_eval->exprArrayData.size();i++){
                        auto expr_arg = expr_to_eval->exprArrayData[i];
                        ASTType *_id = evalExprForTypeId(expr_arg,symbolTableContext);
                        if(!_id){
                            return nullptr;
                            break;
                        };
                        auto & param_decl_pair = *param_decls_it;
                        if(!param_decls_it->getValue()->match(_id,[&](const llvm::formatv_object_base & message){
                            errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("{0}\nContext: Param in invocation of func `{1}`",message,func_id->val),expr_arg);
                        })){
                            return nullptr;
                            break;
                        };
                        ++param_decls_it;
                    };
                    /// return return-type
                    type = funcData->returnType;
                };
                
                /// 1. Eval Args.
                
                
                break;
            }
            default:
                return nullptr;
                break;
        }
        return type;
    }

    bool SemanticA::typeMatches(ASTType *type,ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext){
        
        auto other_type_id = evalExprForTypeId(expr_to_eval,symbolTableContext);
        if(!other_type_id){
            return false;
        };
        
        /// For Now only Matches Type by Name
        /// TODO: Eventually Match by Type Args, Scope, etc..
        ///
        return type->match(other_type_id,[&](const llvm::formatv_object_base & message){
            auto diag = new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("{0}\nContext: Type `{1}` was implied from var initializer",message,other_type_id),expr_to_eval);
        });
    };

    bool SemanticA::checkSymbolsForStmtInScope(ASTStmt *stmt,Semantics::STableContext & symbolTableContext,ASTScope *scope,llvm::Optional<Semantics::SymbolTable> tempSTable){
        if(stmt->type & DECL){
            ASTDecl *decl = (ASTDecl *)stmt;
            switch (decl->type) {
                /// VarDecl
                case VAR_DECL : {
                    auto varDecl = (ASTVarDecl *)decl;
                    for(auto & spec : varDecl->specs){
                        if(!(!spec.type) && (!spec.expr)){
                            // Type Decl and Type Implication Comparsion
                            if(!typeMatches(spec.type,spec.expr,symbolTableContext))
                            {
                                return false;
                            }
                        }
                        else if((!spec.type) && (!spec.expr)) {
                            /// No Type Implication nor any type decl
                            auto id = spec.id;
                            errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Variable `{0}` has no type assignment nor an initial value thus the variable's type cannot be deduced.",id->val),varDecl);
                            return false;
                        }
                        else {
                            /// Type Implication Only.
                            auto type = evalExprForTypeId(spec.expr,symbolTableContext);
                            if(!type){
                                return false;
                            };
                            spec.type = type;
                        };
                    };
                    break;
                }
                /// FuncDecl
                case FUNC_DECL : {
                    std::cout << "FuncDecl" << std::endl;
                    auto funcNode = (ASTFuncDecl *)decl;
                    
                    SemanticsContext ctxt {errStream,funcNode};

                    ASTIdentifier *func_id = funcNode->funcId;
                    
                    /// Ensure that Function declared is unique within the current scope.
                    auto symEntry = symbolTableContext.main->symbolExists(func_id->val,ASTScopeGlobal);
                    if(symEntry){
                        return false;
                    };
                    
                    std::cout << "Func is Unique" << std::endl;

                    for(auto & paramPair : funcNode->params){
                        if(!typeExists(paramPair.getSecond(),symbolTableContext)){
                            return false;
                            break;
                        };
                    };


                    ASTType *return_type_implied = evalBlockStmtForASTType(funcNode->blockStmt,symbolTableContext,&funcNode->params);
                    if(!return_type_implied){
                        return false;
                    };
                    /// Implied Type and Declared Type Comparison.
                    if(funcNode->returnType != nullptr){
                        if(!funcNode->returnType->match(return_type_implied,[&](const llvm::formatv_object_base & message){
                            errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("{0}\nContext: Declared return type of func `{1}` does not match implied return type.",func_id->val),funcNode);
                        }))
                            return false;
                    }
                    /// Assume return type is implied type.
                    else {
                        funcNode->returnType = return_type_implied;
                    };

                    break;
                }
            }
        }
        else if(stmt->type & EXPR){
            ASTExpr *expr = (ASTExpr *)stmt;
            switch (expr->type) {
                case IVKE_EXPR:
                {
                    /// An Invoke Expression with no return variable capture..
                    /// Example:
                    ///
                    /// func myFunc(){
                    ///     // Do Stuff Here
                    /// }
                    ///
                    /// myfunc()
                    ///
                    
                    ASTType *return_type = evalExprForTypeId(expr,symbolTableContext);
                    if(!return_type){
                        return false;
                    };
                    
                    if(return_type != VOID_TYPE){
                        errStream << new SemanticADiagnostic(SemanticADiagnostic::Warning,llvm::formatv("Func `{0}` returns a value but is not being stored by a variable.",expr->id->val),expr);
                        return false;
                        /// Warn of non void object not being stored after creation from function invocation.
                    };
                    
                    break;
                }
                default:
                    break;
            }
        };
        return true;
    };

    bool SemanticA::checkSymbolsForStmt(ASTStmt *stmt, Semantics::STableContext &symbolTableContext){
        return checkSymbolsForStmtInScope(stmt,symbolTableContext,ASTScopeGlobal);
    };

    ASTType *SemanticA::evalBlockStmtForASTType(ASTBlockStmt *stmt,Semantics::STableContext & symbolTableContext,llvm::DenseMap<ASTIdentifier *,ASTType *> *args,bool inFuncContext){
        ASTType *returnType = VOID_TYPE;

        #define RETURN() if(stmt->parentScope->type == ASTScope::Function)\
            return returnType;\
            else\
            return nullptr;
        RETURN()
    };
};

