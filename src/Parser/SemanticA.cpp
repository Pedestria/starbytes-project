#include "starbytes/Parser/SemanticA.h"
#include "starbytes/Parser/SymTable.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTNodes.def"
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/FormatVariadic.h>
#include <iostream>

namespace starbytes {

    SemanticA::SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticBufferedLogger & errStream):syntaxARef(syntaxARef),errStream(errStream){
            
    }

    void SemanticA::start(){
        std::cout << "Starting SemanticA" << std::endl;
    }

    void SemanticA::finish(){
        
    }
     /// Only registers new symbols associated with top level decls!
    void SemanticA::addSTableEntryForDecl(ASTDecl *decl,Semantics::SymbolTable *tablePtr){
       
        auto buildVarEntry = [&](ASTVarDecl::VarSpec &spec){
            auto *e = new Semantics::SymbolTable::Entry();
            auto data = new Semantics::SymbolTable::Var();
            data->type = spec.type;
            auto id = spec.id;
            e->name = id->val;
            e->data = data;
            e->type = Semantics::SymbolTable::Entry::Var;
            return e;
        };
        
        auto buildFuncEntry = [&](ASTFuncDecl *func){
            auto *e = new Semantics::SymbolTable::Entry();
            auto data = new Semantics::SymbolTable::Function();
            data->returnType = func->returnType;
            data->funcType = func->funcType;
            
            for(auto & param_pair : func->params){
                data->paramMap.insert(std::make_pair(param_pair.getFirst()->val,param_pair.getSecond()));
            };
            
            e->name = func->funcId->val;
            e->data = data;
            e->type = Semantics::SymbolTable::Entry::Function;
            return e;
        };
        
        switch (decl->type) {
            case VAR_DECL : {
                auto *varDecl = (ASTVarDecl *)decl;
                for(auto & spec : varDecl->specs) {
                    tablePtr->addSymbolInScope(buildVarEntry(spec),varDecl->scope);
                }
                break;
            }
            case FUNC_DECL : {
                auto *funcDecl = (ASTFuncDecl *)decl;
                tablePtr->addSymbolInScope(buildFuncEntry(funcDecl),decl->scope);
                break;
            }
             case CLASS_DECL : {
                 auto classDecl = (ASTClassDecl *)decl;
                 Semantics::SymbolTable::Entry *e = new Semantics::SymbolTable::Entry();
                 e->name = classDecl->id->val;
                 e->type = Semantics::SymbolTable::Entry::Class;
                 auto data = new Semantics::SymbolTable::Class();
                 e->data = data;
                 data->classType = classDecl->classType;
                 for(auto & f : classDecl->fields){
                     for(auto & v_spec : f->specs){
                         data->fields.push_back(new Semantics::SymbolTable::Var {v_spec.type});
                     }
                 }
                 for(auto & m : classDecl->methods){
                     data->instMethods.push_back(new Semantics::SymbolTable::Function {m->returnType,m->funcType});
                 }
                 tablePtr->addSymbolInScope(e,decl->scope);
                 break;
             }
        default : {
            break;
        }
        }
    }


    bool SemanticA::typeExists(ASTType *type,Semantics::STableContext &contextTableContext,ASTScope *scope){
        /// First check to see if ASTType is one of the built in types
        if(type == VOID_TYPE || type == STRING_TYPE || type == ARRAY_TYPE || 
        type == DICTIONARY_TYPE || type == BOOL_TYPE || type == INT_TYPE){
            return true;
        }
        else {
            return false;
        }
    }



    bool SemanticA::typeMatches(ASTType *type,ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext,ASTScopeSemanticsContext & scopeContext){
        
        auto other_type_id = evalExprForTypeId(expr_to_eval,symbolTableContext,scopeContext);
        if(!other_type_id){
            return false;
        };
        
        /// For Now only Matches Type by Name
        /// TODO: Eventually Match by Type Args, Scope, etc..
        ///
        return type->match(other_type_id,[&](const llvm::formatv_object_base & message){
            errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("{0}\nContext: Type `{1}` was implied from var initializer",message,other_type_id),expr_to_eval);
        });
    }



    bool SemanticA::checkSymbolsForStmtInScope(ASTStmt *stmt,Semantics::STableContext & symbolTableContext,ASTScope *scope,llvm::Optional<Semantics::SymbolTable> tempSTable){
        ASTScopeSemanticsContext scopeContext {scope};
        if(stmt->type & DECL){
            auto *decl = (ASTDecl *)stmt;
            bool hasErrored;
            auto rc = evalGenericDecl(decl,symbolTableContext,scopeContext,&hasErrored);
            if(hasErrored && !rc)
                switch (decl->type) {
                    /// FuncDecl
                    case FUNC_DECL : {
    //                    std::cout << "FuncDecl" << std::endl;
                        auto funcNode = (ASTFuncDecl *)decl;
                        

                        ASTIdentifier *func_id = funcNode->funcId;
                        
                        /// Ensure that Function declared is unique within the current scope.
                        auto symEntry = symbolTableContext.main->symbolExists(func_id->val,scope);
                        if(symEntry){
                            return false;
                        };
                        
    //                    std::cout << "Func is Unique" << std::endl;

                        for(auto & paramPair : funcNode->params){
                            if(!typeExists(paramPair.getSecond(),symbolTableContext,scope)){
                                return false;
                                break;
                            };
                        };

                        bool hasFailed;

                        ASTScopeSemanticsContext funcScopeContext {funcNode->blockStmt->parentScope,&funcNode->params};

                        ASTType *return_type_implied = evalBlockStmtForASTType(funcNode->blockStmt,symbolTableContext,&hasFailed,funcScopeContext,true);
                        if(!return_type_implied && hasFailed){
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
                    case CLASS_DECL : {
                        if(scope->type != ASTScope::Namespace && scope->type != ASTScope::Neutral){
                            errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Class decl not allowed in class scope"),decl);
                            return false;
                        }
                        auto classDecl = (ASTClassDecl *)decl;
                        
                        auto symEntry = symbolTableContext.main->symbolExists(classDecl->id->val, scope);
                        
                        if(symEntry){
                            return false;
                        }
                        
                        bool hasErrored;
                        ASTScopeSemanticsContext scopeContext {classDecl->scope,nullptr};
                        for(auto & f : classDecl->fields){
                            auto rc = evalGenericDecl(f,symbolTableContext, scopeContext, &hasErrored);
                        }
                        
                        for(auto & m : classDecl->methods){
                            auto rc = checkSymbolsForStmtInScope(m, symbolTableContext,classDecl->scope);
                            if(!rc){
                                return false;
                            }
                        }
                        
                        
                        return true;
                        break;
                    }
                    case RETURN_DECL : {
                        errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Return can not be declared in a namespace scope"),stmt);
                        return false;
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
                    
                    ASTType *return_type = evalExprForTypeId(expr,symbolTableContext,scopeContext);
                    // std::cout << "Log" << return_type << std::endl;
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
    }

    bool SemanticA::checkSymbolsForStmt(ASTStmt *stmt, Semantics::STableContext &symbolTableContext){
        return checkSymbolsForStmtInScope(stmt,symbolTableContext,ASTScopeGlobal);
    }

    
}

