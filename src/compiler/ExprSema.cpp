#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/SemanticA.h"

namespace starbytes {

 #define PRINT_FUNC_ID "print"
  #define EXIT_FUNC_ID "exit"

auto print_func_type = ASTType::Create(PRINT_FUNC_ID,nullptr);
auto exit_func_type = ASTType::Create(EXIT_FUNC_ID,nullptr);
auto scope_ref_type = ASTType::Create("__scope__",nullptr);

ASTType *isBuiltinType(ASTIdentifier *id){
    if(id->val == PRINT_FUNC_ID) return print_func_type;
    if(id->val == EXIT_FUNC_ID)  return exit_func_type;
    return nullptr;
}

static Semantics::SymbolTable::Entry *findClassEntry(Semantics::STableContext &symbolTableContext,
                                                     string_ref className,
                                                     std::shared_ptr<ASTScope> scope){
    auto *entry = symbolTableContext.findEntryNoDiag(className,scope);
    if(entry && entry->type == Semantics::SymbolTable::Entry::Class){
        return entry;
    }
    entry = symbolTableContext.findEntryByEmittedNoDiag(className);
    if(entry && entry->type == Semantics::SymbolTable::Entry::Class){
        return entry;
    }
    return nullptr;
}

static Semantics::SymbolTable::Function *findClassMethod(Semantics::SymbolTable::Class *classData,
                                                         string_ref methodName){
    for(auto *method : classData->instMethods){
        if(method->name == methodName){
            return method;
        }
    }
    return nullptr;
}

static Semantics::SymbolTable::Var *findClassField(Semantics::SymbolTable::Class *classData,
                                                   string_ref fieldName){
    for(auto *field : classData->fields){
        if(field->name == fieldName){
            return field;
        }
    }
    return nullptr;
}

static std::shared_ptr<ASTScope> scopeFromEntry(Semantics::SymbolTable::Entry *entry){
    if(!entry || entry->type != Semantics::SymbolTable::Entry::Scope || !entry->data){
        return nullptr;
    }
    return *((std::shared_ptr<ASTScope> *)entry->data);
}


ASTType * SemanticA::evalExprForTypeId(ASTExpr *expr_to_eval,
                                       Semantics::STableContext & symbolTableContext,
                                       ASTScopeSemanticsContext & scopeContext){
        ASTType *type;
        switch (expr_to_eval->type) {
            case ID_EXPR : {
                ASTIdentifier *id_ = expr_to_eval->id;
                SemanticsContext ctxt {errStream,expr_to_eval};

                auto _is_builtin_type = isBuiltinType(id_);
                /// 1. Check and see if id is a builtin func.
                if(_is_builtin_type){
                    id_->type = ASTIdentifier::Function;
                    return _is_builtin_type;
                };

                /// 2. Check if scope context has args.. (In Function Context)

                if(scopeContext.scope->type == ASTScope::Function && scopeContext.args != nullptr){
                    for(auto & __arg : *scopeContext.args){
                        if(__arg.first->match(id_)){
                            return __arg.second;
                            break;
                        };
                    };
                    
                };

                /// 3. Else, do normal symbol matching.

                auto symbol_ = symbolTableContext.findEntry(id_->val,ctxt,scopeContext.scope);
                // std::cout << "SYMBOL_PTR:" << symbol_ << std::endl;
                if(!symbol_){
                    return nullptr;
                    break;
                };
                
                if(symbol_->type == Semantics::SymbolTable::Entry::Var){
                    auto varData = (Semantics::SymbolTable::Var *)symbol_->data;
                    id_->type = ASTIdentifier::Var;
                    if(!symbol_->emittedName.empty()){
                        id_->val = symbol_->emittedName;
                    }
                    type = varData->type;
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::Function){
                    auto funcData = (Semantics::SymbolTable::Function *)symbol_->data;
                    id_->type = ASTIdentifier::Function;
                    if(!symbol_->emittedName.empty()){
                        id_->val = symbol_->emittedName;
                    }
                    type = funcData->funcType;
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::Class){
                    auto classData = (Semantics::SymbolTable::Class *)symbol_->data;
                    id_->type = ASTIdentifier::Class;
                    if(!symbol_->emittedName.empty()){
                        id_->val = symbol_->emittedName;
                    }
                    type = classData->classType;
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::Scope){
                    id_->type = ASTIdentifier::Scope;
                    type = scope_ref_type;
                }
                else {
                    errStream.push(SemanticADiagnostic::create("Identifier in this context cannot identify any other symbol type except another variable.",expr_to_eval,Diagnostic::Error));
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
            case NUM_LITERAL : {
                auto *literal = (ASTLiteralExpr *)expr_to_eval;
                /// If is Integer
                if(literal->intValue.has_value()){
                    type = INT_TYPE;
                }
                /// Else it is a Floating Point
                else {
                    type = FLOAT_TYPE;
                };
                break;
            }
            case MEMBER_EXPR : {
                if(!expr_to_eval->leftExpr || !expr_to_eval->rightExpr || !expr_to_eval->rightExpr->id){
                    errStream.push(SemanticADiagnostic::create("Malformed member expression.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                ASTType *leftType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                if(!leftType){
                    return nullptr;
                }

                std::shared_ptr<ASTScope> accessScope = nullptr;
                if(expr_to_eval->leftExpr->type == ID_EXPR && expr_to_eval->leftExpr->id &&
                   expr_to_eval->leftExpr->id->type == ASTIdentifier::Scope){
                    auto *leftScopeEntry = symbolTableContext.findEntryNoDiag(expr_to_eval->leftExpr->id->val,scopeContext.scope);
                    accessScope = scopeFromEntry(leftScopeEntry);
                }
                else if(expr_to_eval->leftExpr->type == MEMBER_EXPR){
                    auto *leftMember = expr_to_eval->leftExpr;
                    if(leftMember->isScopeAccess && leftMember->resolvedScope){
                        accessScope = leftMember->resolvedScope;
                    }
                }

                auto memberName = expr_to_eval->rightExpr->id->val;
                if(accessScope){
                    auto *scopeMemberEntry = symbolTableContext.findEntryInExactScopeNoDiag(memberName,accessScope);
                    if(!scopeMemberEntry){
                        errStream.push(SemanticADiagnostic::create("Unknown symbol in scope member access.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    expr_to_eval->isScopeAccess = true;
                    expr_to_eval->resolvedScope = accessScope;
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Scope){
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Scope;
                        expr_to_eval->resolvedScope = scopeFromEntry(scopeMemberEntry);
                        type = scope_ref_type;
                        break;
                    }
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Var){
                        auto *varData = (Semantics::SymbolTable::Var *)scopeMemberEntry->data;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
                        type = varData->type;
                        break;
                    }
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Function){
                        auto *funcData = (Semantics::SymbolTable::Function *)scopeMemberEntry->data;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                        type = funcData->funcType;
                        break;
                    }
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Class){
                        auto *classData = (Semantics::SymbolTable::Class *)scopeMemberEntry->data;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Class;
                        type = classData->classType;
                        break;
                    }
                    errStream.push(SemanticADiagnostic::create("Unsupported scope member type.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }

                auto classEntry = findClassEntry(symbolTableContext,leftType->getName(),scopeContext.scope);
                if(!classEntry){
                    errStream.push(SemanticADiagnostic::create("Member access requires class instance type.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                if(auto *field = findClassField(classData,memberName)){
                    expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
                    type = field->type;
                    break;
                }
                if(auto *method = findClassMethod(classData,memberName)){
                    expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                    type = method->funcType;
                    break;
                }
                errStream.push(SemanticADiagnostic::create("Unknown class member.",expr_to_eval,Diagnostic::Error));
                return nullptr;
            }
            case ASSIGN_EXPR : {
                if(!expr_to_eval->leftExpr || !expr_to_eval->rightExpr){
                    errStream.push(SemanticADiagnostic::create("Malformed assignment expression.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                ASTType *lhsType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                ASTType *rhsType = evalExprForTypeId(expr_to_eval->rightExpr,symbolTableContext,scopeContext);
                if(!lhsType || !rhsType){
                    return nullptr;
                }
                if(!lhsType->match(rhsType,[&](std::string){
                    errStream.push(SemanticADiagnostic::create("Assignment type mismatch.",expr_to_eval,Diagnostic::Error));
                })){
                    return nullptr;
                }

                if(expr_to_eval->leftExpr->type == MEMBER_EXPR){
                    auto *memberExpr = expr_to_eval->leftExpr;
                    if(memberExpr->isScopeAccess){
                        errStream.push(SemanticADiagnostic::create("Assignment to scope-qualified symbols is not supported.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    ASTType *baseType = evalExprForTypeId(memberExpr->leftExpr,symbolTableContext,scopeContext);
                    if(!baseType){
                        return nullptr;
                    }
                    auto classEntry = findClassEntry(symbolTableContext,baseType->getName(),scopeContext.scope);
                    if(classEntry){
                        auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                        auto *field = findClassField(classData,memberExpr->rightExpr->id->val);
                        if(field && field->isReadonly){
                            errStream.push(SemanticADiagnostic::create("Cannot assign to @readonly field.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                    }
                }
                type = lhsType;
                break;
            }
            case IVKE_EXPR : {
//                std::cout << expr_to_eval->callee->type << std::endl;
                if(!expr_to_eval->callee){
                    return nullptr;
                }
                ASTType *funcType = evalExprForTypeId(expr_to_eval->callee, symbolTableContext,scopeContext);
                if(!funcType){
                    return nullptr;
                }

                if(!expr_to_eval->isConstructorCall &&
                   expr_to_eval->callee->type == MEMBER_EXPR && expr_to_eval->callee->isScopeAccess){
                    auto *memberExpr = expr_to_eval->callee;
                    if(!memberExpr->resolvedScope){
                        return nullptr;
                    }
                    auto *entry = symbolTableContext.findEntryInExactScopeNoDiag(memberExpr->rightExpr->id->val,memberExpr->resolvedScope);
                    if(!entry || entry->type != Semantics::SymbolTable::Entry::Function){
                        errStream.push(SemanticADiagnostic::create("Scope member invocation requires a function symbol.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    auto *funcData = (Semantics::SymbolTable::Function *)entry->data;
                    if(expr_to_eval->exprArrayData.size() != funcData->paramMap.size()){
                        errStream.push(SemanticADiagnostic::create("Incorrect number of function arguments.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    auto paramIt = funcData->paramMap.begin();
                    for(auto *arg : expr_to_eval->exprArrayData){
                        auto *argType = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!argType){
                            return nullptr;
                        }
                        if(!paramIt->second->match(argType,[&](std::string){
                            errStream.push(SemanticADiagnostic::create("Function argument type mismatch.",arg,Diagnostic::Error));
                        })){
                            return nullptr;
                        }
                        ++paramIt;
                    }
                    type = funcData->returnType ? funcData->returnType : VOID_TYPE;
                    break;
                }

                if(!expr_to_eval->isConstructorCall && expr_to_eval->callee->type == MEMBER_EXPR){
                    auto *memberExpr = expr_to_eval->callee;
                    ASTType *baseType = evalExprForTypeId(memberExpr->leftExpr,symbolTableContext,scopeContext);
                    if(!baseType){
                        return nullptr;
                    }
                    auto classEntry = findClassEntry(symbolTableContext,baseType->getName(),scopeContext.scope);
                    if(!classEntry){
                        return nullptr;
                    }
                    auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                    auto *method = findClassMethod(classData,memberExpr->rightExpr->id->val);
                    if(!method){
                        errStream.push(SemanticADiagnostic::create("Unknown method.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(expr_to_eval->exprArrayData.size() != method->paramMap.size()){
                        errStream.push(SemanticADiagnostic::create("Incorrect number of method arguments.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    auto paramIt = method->paramMap.begin();
                    for(auto *arg : expr_to_eval->exprArrayData){
                        auto *argType = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!argType){
                            return nullptr;
                        }
                        if(!paramIt->second->match(argType,[&](std::string){
                            errStream.push(SemanticADiagnostic::create("Method argument type mismatch.",arg,Diagnostic::Error));
                        })){
                            return nullptr;
                        }
                        ++paramIt;
                    }
                    type = method->returnType ? method->returnType : VOID_TYPE;
                    break;
                }

                if(expr_to_eval->callee->type == ID_EXPR && expr_to_eval->callee->id &&
                   expr_to_eval->callee->id->type == ASTIdentifier::Class){
                    for(auto & arg : expr_to_eval->exprArrayData){
                        ASTType *_id = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!_id){
                            return nullptr;
                        }
                    }
                    type = funcType;
                    break;
                }
                if(expr_to_eval->callee->type == MEMBER_EXPR && expr_to_eval->callee->isScopeAccess &&
                   expr_to_eval->callee->rightExpr && expr_to_eval->callee->rightExpr->id &&
                   expr_to_eval->callee->rightExpr->id->type == ASTIdentifier::Class){
                    for(auto & arg : expr_to_eval->exprArrayData){
                        ASTType *_id = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!_id){
                            return nullptr;
                        }
                    }
                    type = funcType;
                    break;
                }
                /// a. Check internal function return types.
                string_ref func_name = funcType->getName();
                if(func_name == PRINT_FUNC_ID){
                    if(expr_to_eval->exprArrayData.size() > 1){
                        errStream.push(SemanticADiagnostic::create("Incorrect number of arguments",expr_to_eval,Diagnostic::Error));
                    }
                    /// print() -> Void
                    type = VOID_TYPE;
                 
                    for(auto & arg : expr_to_eval->exprArrayData){
                        ASTType *_id = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!_id){
                            return nullptr;
                            break;
                        };
                        
                    };
                    // std::cout << "Print Func 2" << std::endl;
                }
                else {
                    /// Check to see if function was previously defined.
                    SemanticsContext ctxt {errStream,expr_to_eval};
                    auto entry = symbolTableContext.findEntry(func_name,ctxt,scopeContext.scope);
                    if(!entry){
                        return nullptr;
                        break;
                    };
                    /// Check to see if symbol is function.
                    if(entry->type != Semantics::SymbolTable::Entry::Function){
                        //errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Identifier `{0}` does not identify a function",entry->name),expr_to_eval);
                        return nullptr;
                        break;
                    };

                    auto funcData = (Semantics::SymbolTable::Function *)entry->data;
                    
                    if(expr_to_eval->exprArrayData.size() != funcData->paramMap.size()){
                        //errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Incorrect number of arguments. Expected {0} args, but got {1}\nContext: Invocation of func `{2}`",funcData->paramMap.size(),expr_to_eval->exprArrayData.size(),funcType->getName()),expr_to_eval);
                        return nullptr;
                        break;
                    }
                    
                    auto param_decls_it = funcData->paramMap.begin();
                    for(unsigned i = 0;i < expr_to_eval->exprArrayData.size();i++){
                        auto expr_arg = expr_to_eval->exprArrayData[i];
                        ASTType *_id = evalExprForTypeId(expr_arg,symbolTableContext,scopeContext);
                        if(!_id){
                            return nullptr;
                            break;
                        };
                        auto & param_decl_pair = *param_decls_it;
                        if(!param_decls_it->second->match(_id,[&](std::string message){
                            /// errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("{0}\nContext: Param in invocation of func `{1}`",message,funcType->getName()),expr_arg);
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

}
