#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/SemanticA.h"

namespace starbytes {

 #define PRINT_FUNC_ID "print"
  #define EXIT_FUNC_ID "exit"

auto print_func_type = ASTType::Create(PRINT_FUNC_ID,nullptr);
auto exit_func_type = ASTType::Create(EXIT_FUNC_ID,nullptr);

ASTType *isBuiltinType(ASTIdentifier *id){
    if(id->val == PRINT_FUNC_ID){
        return print_func_type;
    };
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

                if(scopeContext.scope->type == ASTScope::Function){
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
                    type = varData->type;
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::Function){
                    auto funcData = (Semantics::SymbolTable::Function *)symbol_->data;
                    id_->type = ASTIdentifier::Function;
                    type = funcData->funcType;
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
            case IVKE_EXPR : {
//                std::cout << expr_to_eval->callee->type << std::endl;
                
                /// 2. Check return type.
                ASTType *funcType = evalExprForTypeId(expr_to_eval->callee, symbolTableContext,scopeContext);
                if(!funcType){
                    return nullptr;
                };
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
