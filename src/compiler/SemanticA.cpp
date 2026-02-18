#include "starbytes/compiler/SemanticA.h"
#include "starbytes/compiler/SymTable.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTNodes.def"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>

namespace starbytes {

    static bool attributeArgIsString(const ASTAttributeArg &arg){
        if(!arg.value){
            return false;
        }
        auto *expr = (ASTExpr *)arg.value;
        if(expr->type != STR_LITERAL){
            return false;
        }
        auto *literal = (ASTLiteralExpr *)expr;
        return literal->strValue.has_value();
    }

    static bool validateSystemAttribute(ASTDecl *decl,
                                        const ASTAttribute &attr,
                                        DiagnosticHandler &errStream){
        auto pushAttrError = [&](const std::string &msg){
            errStream.push(SemanticADiagnostic::create(msg,decl,Diagnostic::Error));
        };

        if(attr.name == "readonly"){
            if(decl->type != VAR_DECL || decl->scope->type != ASTScope::Class){
                pushAttrError("@readonly is only valid on class fields.");
                return false;
            }
            if(!attr.args.empty()){
                pushAttrError("@readonly does not accept arguments.");
                return false;
            }
            return true;
        }

        if(attr.name == "deprecated"){
            if(!(decl->type == VAR_DECL || decl->type == FUNC_DECL || decl->type == CLASS_DECL)){
                pushAttrError("@deprecated is only valid on class/function/field declarations.");
                return false;
            }
            if(attr.args.empty()){
                return true;
            }
            if(attr.args.size() != 1 || !attributeArgIsString(attr.args[0])){
                pushAttrError("@deprecated accepts at most one string argument.");
                return false;
            }
            if(attr.args[0].key.has_value() && attr.args[0].key.value() != "message"){
                pushAttrError("@deprecated only accepts named argument `message`.");
                return false;
            }
            return true;
        }

        if(attr.name == "native"){
            if(!(decl->type == FUNC_DECL || decl->type == CLASS_DECL)){
                pushAttrError("@native is only valid on class/function declarations.");
                return false;
            }
            if(attr.args.size() != 1 || !attributeArgIsString(attr.args[0])){
                pushAttrError("@native requires one string argument.");
                return false;
            }
            if(attr.args[0].key.has_value() && attr.args[0].key.value() != "name"){
                pushAttrError("@native only accepts named argument `name`.");
                return false;
            }
            return true;
        }

        return true;
    }

    static bool validateAttributesForDecl(ASTDecl *decl, DiagnosticHandler &errStream){
        for(auto &attr : decl->attributes){
            if(attr.name == "readonly" || attr.name == "deprecated" || attr.name == "native"){
                if(!validateSystemAttribute(decl,attr,errStream)){
                    return false;
                }
            }
        }
        return true;
    }

    static std::vector<std::string> getNamespacePath(std::shared_ptr<ASTScope> scope){
        std::vector<std::string> path;
        for(auto s = scope; s != nullptr; s = s->parentScope){
            if(s->type == ASTScope::Namespace){
                path.push_back(s->name);
            }
        }
        std::reverse(path.begin(),path.end());
        return path;
    }

    static std::string buildEmittedName(std::shared_ptr<ASTScope> scope,const std::string &symbolName){
        auto nsPath = getNamespacePath(scope);
        if(nsPath.empty()){
            return symbolName;
        }
        std::ostringstream out;
        for(size_t i = 0;i < nsPath.size();++i){
            if(i > 0){
                out << "__";
            }
            out << nsPath[i];
        }
        out << "__" << symbolName;
        return out.str();
    }

    DiagnosticPtr SemanticADiagnostic::create(string_ref message, ASTStmt *stmt, Type ty){
        std::ostringstream new_message;
        stmt->printToStream(new_message);
        new_message << " " << message;
        auto msg = new_message.str();
        return StandardDiagnostic::createError(msg);
    }

    SemanticA::SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticHandler & errStream):syntaxARef(syntaxARef),errStream(errStream){
            
    }

    void SemanticA::start(){
    }

    void SemanticA::finish(){
        
    }
     /// Only registers new symbols associated with top level decls!
    void SemanticA::addSTableEntryForDecl(ASTDecl *decl,Semantics::SymbolTable *tablePtr){
       
        auto buildVarEntry = [&](ASTVarDecl::VarSpec &spec,std::shared_ptr<ASTScope> scope){
            std::string sourceName = spec.id->val;
            std::string emittedName = buildEmittedName(scope,sourceName);
            auto *e = new Semantics::SymbolTable::Entry();
            auto data = new Semantics::SymbolTable::Var();
            data->name = sourceName;
            data->type = spec.type;
            e->name = sourceName;
            e->emittedName = emittedName;
            e->data = data;
            e->type = Semantics::SymbolTable::Entry::Var;
            spec.id->val = emittedName;
            return e;
        };
        
        auto buildFuncEntry = [&](ASTFuncDecl *func,std::shared_ptr<ASTScope> scope){
            std::string sourceName = func->funcId->val;
            std::string emittedName = buildEmittedName(scope,sourceName);
            auto *e = new Semantics::SymbolTable::Entry();
            auto data = new Semantics::SymbolTable::Function();
            data->name = sourceName;
            data->returnType = func->returnType;
            data->funcType = func->funcType;
            
            for(auto & param_pair : func->params){
                data->paramMap.insert(std::make_pair(param_pair.first->val,param_pair.second));
            };
            
            e->name = sourceName;
            e->emittedName = emittedName;
            e->data = data;
            e->type = Semantics::SymbolTable::Entry::Function;
            func->funcId->val = emittedName;
            return e;
        };
        
        switch (decl->type) {
            case VAR_DECL : {
                auto *varDecl = (ASTVarDecl *)decl;
                for(auto & spec : varDecl->specs) {
                    tablePtr->addSymbolInScope(buildVarEntry(spec,varDecl->scope),varDecl->scope);
                }
                break;
            }
            case FUNC_DECL : {
                auto *funcDecl = (ASTFuncDecl *)decl;
                tablePtr->addSymbolInScope(buildFuncEntry(funcDecl,decl->scope),decl->scope);
                break;
            }
             case CLASS_DECL : {
                 auto classDecl = (ASTClassDecl *)decl;
                 std::string sourceName = classDecl->id->val;
                 std::string emittedName = buildEmittedName(decl->scope,sourceName);
                 Semantics::SymbolTable::Entry *e = new Semantics::SymbolTable::Entry();
                 e->name = sourceName;
                 e->emittedName = emittedName;
                 e->type = Semantics::SymbolTable::Entry::Class;
                 auto data = new Semantics::SymbolTable::Class();
                 e->data = data;
                 data->classType = ASTType::Create(emittedName.c_str(),classDecl,false,false);
                 classDecl->classType = data->classType;
                 classDecl->id->val = emittedName;
                 for(auto & f : classDecl->fields){
                     bool readonlyField = false;
                     for(auto &attr : f->attributes){
                         if(attr.name == "readonly"){
                             readonlyField = true;
                             break;
                         }
                     }
                     for(auto & v_spec : f->specs){
                         auto *field = new Semantics::SymbolTable::Var();
                         field->name = v_spec.id->val;
                         field->type = v_spec.type;
                         field->isReadonly = readonlyField;
                         data->fields.push_back(field);
                     }
                 }
                 for(auto & m : classDecl->methods){
                     auto *method = new Semantics::SymbolTable::Function();
                     method->name = m->funcId->val;
                     method->returnType = m->returnType;
                     method->funcType = m->funcType;
                     for(auto & param_pair : m->params){
                         method->paramMap.insert(std::make_pair(param_pair.first->val,param_pair.second));
                     }
                     data->instMethods.push_back(method);
                 }
                 for(auto & c : classDecl->constructors){
                     auto *ctor = new Semantics::SymbolTable::Function();
                     ctor->name = "__ctor__" + std::to_string(c->params.size());
                     ctor->returnType = VOID_TYPE;
                     ctor->funcType = data->classType;
                     for(auto &param_pair : c->params){
                         ctor->paramMap.insert(std::make_pair(param_pair.first->val,param_pair.second));
                     }
                     data->constructors.push_back(ctor);
                 }
                 tablePtr->addSymbolInScope(e,decl->scope);
                 break;
             }
            case SCOPE_DECL : {
                auto *scopeDecl = (ASTScopeDecl *)decl;
                auto *entry = new Semantics::SymbolTable::Entry();
                entry->name = scopeDecl->scopeId->val;
                entry->emittedName = entry->name;
                entry->type = Semantics::SymbolTable::Entry::Scope;
                entry->data = new std::shared_ptr<ASTScope>(scopeDecl->blockStmt->parentScope);
                tablePtr->addSymbolInScope(entry,decl->scope);
                break;
            }
        default : {
            break;
        }
        }
    }


    bool SemanticA::typeExists(ASTType *type,Semantics::STableContext &contextTableContext,std::shared_ptr<ASTScope> scope){
        /// First check to see if ASTType is one of the built in types
        if(!type){
            return false;
        }
        if(type->nameMatches(VOID_TYPE) ||
           type->nameMatches(STRING_TYPE) ||
           type->nameMatches(ARRAY_TYPE) ||
           type->nameMatches(DICTIONARY_TYPE) ||
           type->nameMatches(BOOL_TYPE) ||
           type->nameMatches(INT_TYPE) ||
           type->nameMatches(FLOAT_TYPE)){
            return true;
        }
        auto *entry = contextTableContext.findEntryNoDiag(type->getName(),scope);
        if(entry && entry->type == Semantics::SymbolTable::Entry::Class){
            return true;
        }
        return false;
    }



    bool SemanticA::typeMatches(ASTType *type,ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext,ASTScopeSemanticsContext & scopeContext){
        
        auto other_type_id = evalExprForTypeId(expr_to_eval,symbolTableContext,scopeContext);
        if(!other_type_id){
            return false;
        };
        
        /// For Now only Matches Type by Name
        /// TODO: Eventually Match by Type Args, Scope, etc..
        ///
        return type->match(other_type_id,[&](std::string message){
            std::ostringstream ss;
            ss << message << "\nContext: Type `" << other_type_id->getName() << "`was implied from var initializer";
            auto res = ss.str();
            errStream.push(SemanticADiagnostic::create(res,expr_to_eval,Diagnostic::Error));
        });
    }



    bool SemanticA::checkSymbolsForStmtInScope(ASTStmt *stmt,Semantics::STableContext & symbolTableContext,std::shared_ptr<ASTScope> scope,std::optional<Semantics::SymbolTable> tempSTable){
        ASTScopeSemanticsContext scopeContext {scope};
        if(stmt->type & DECL){
            auto *decl = (ASTDecl *)stmt;
            if(!validateAttributesForDecl(decl,errStream)){
                return false;
            }
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
                            if(!typeExists(paramPair.second,symbolTableContext,scope)){
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
                            if(!funcNode->returnType->match(return_type_implied,[&](std::string message){
                                std::ostringstream ss;
                                ss << message << "\nContext: Declared return type of func `" << func_id->val << "` does not match implied return type.";
                                auto out = ss.str();
                                errStream.push(SemanticADiagnostic::create(out,funcNode,Diagnostic::Error));
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
                            errStream.push(SemanticADiagnostic::create("Class decl not allowed in class scope",decl,Diagnostic::Error));
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
                            if(!validateAttributesForDecl(f,errStream)){
                                return false;
                            }
                            if(hasErrored && !rc){
                                return false;
                            }
                        }
                        
                        std::set<std::string> methodNames;
                        for(auto & m : classDecl->methods){
                            if(methodNames.find(m->funcId->val) != methodNames.end()){
                                errStream.push(SemanticADiagnostic::create("Duplicate class method name.",m,Diagnostic::Error));
                                return false;
                            }
                            methodNames.insert(m->funcId->val);
                            for(auto &paramPair : m->params){
                                if(!typeExists(paramPair.second,symbolTableContext,scope)){
                                    return false;
                                }
                            }
                            std::map<ASTIdentifier *,ASTType *> methodParams = m->params;
                            auto *selfId = new ASTIdentifier();
                            selfId->val = "self";
                            methodParams.insert(std::make_pair(selfId,classDecl->classType));
                            bool methodHasFailed = false;
                            ASTScopeSemanticsContext methodScopeContext {m->blockStmt->parentScope,&methodParams};
                            ASTType *returnTypeImplied = evalBlockStmtForASTType(m->blockStmt,symbolTableContext,&methodHasFailed,methodScopeContext,true);
                            if(methodHasFailed || !returnTypeImplied){
                                return false;
                            }
                            if(m->returnType != nullptr){
                                if(!m->returnType->match(returnTypeImplied,[&](std::string message){
                                    std::ostringstream ss;
                                    ss << message << "\nContext: Declared return type of class method `" << m->funcId->val << "` does not match implied return type.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),m,Diagnostic::Error));
                                })){
                                    return false;
                                }
                            }
                            else {
                                m->returnType = returnTypeImplied;
                            }
                        }

                        std::set<size_t> ctorArities;
                        for(auto & c : classDecl->constructors){
                            size_t arity = c->params.size();
                            if(ctorArities.find(arity) != ctorArities.end()){
                                errStream.push(SemanticADiagnostic::create("Duplicate constructor arity.",c,Diagnostic::Error));
                                return false;
                            }
                            ctorArities.insert(arity);
                            for(auto &paramPair : c->params){
                                if(!typeExists(paramPair.second,symbolTableContext,scope)){
                                    return false;
                                }
                            }
                            std::map<ASTIdentifier *,ASTType *> ctorParams = c->params;
                            auto *selfId = new ASTIdentifier();
                            selfId->val = "self";
                            ctorParams.insert(std::make_pair(selfId,classDecl->classType));
                            bool ctorHasFailed = false;
                            ASTScopeSemanticsContext ctorScopeContext {c->blockStmt->parentScope,&ctorParams};
                            ASTType *ctorReturnType = evalBlockStmtForASTType(c->blockStmt,symbolTableContext,&ctorHasFailed,ctorScopeContext,true);
                            if(ctorHasFailed || !ctorReturnType){
                                return false;
                            }
                            if(ctorReturnType != VOID_TYPE){
                                errStream.push(SemanticADiagnostic::create("Constructor cannot return a value.",c,Diagnostic::Error));
                                return false;
                            }
                        }
                        
                        
                        return true;
                        break;
                    }
                    case SCOPE_DECL : {
                        if(scope->type == ASTScope::Class || scope->type == ASTScope::Function){
                            errStream.push(SemanticADiagnostic::create("Scope declaration is not allowed in class/function scope.",decl,Diagnostic::Error));
                            return false;
                        }
                        auto *scopeDecl = (ASTScopeDecl *)decl;
                        if(symbolTableContext.findEntryInExactScopeNoDiag(scopeDecl->scopeId->val,scope)){
                            errStream.push(SemanticADiagnostic::create("Duplicate scope name in current scope.",decl,Diagnostic::Error));
                            return false;
                        }
                        for(auto *innerStmt : scopeDecl->blockStmt->body){
                            auto ok = checkSymbolsForStmtInScope(innerStmt,symbolTableContext,scopeDecl->blockStmt->parentScope);
                            if(!ok){
                                return false;
                            }
                            if(innerStmt->type & DECL){
                                addSTableEntryForDecl((ASTDecl *)innerStmt,symbolTableContext.main.get());
                            }
                        }
                        return true;
                    }
                    case RETURN_DECL : {
                        errStream.push(SemanticADiagnostic::create("Return can not be declared in a namespace scope",stmt,Diagnostic::Error));
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
                        std::ostringstream ss;
                        ss << "Func `" << expr->id->val << "` returns a value but is not being stored by a variable.";
                        auto res = ss.str();
                        errStream.push(SemanticADiagnostic::create(res,expr,Diagnostic::Warning));
                        /// Warn of non void object not being stored after creation from function invocation.
                    };
                    
                    break;
                }
                default:
                {
                    ASTType *exprType = evalExprForTypeId(expr,symbolTableContext,scopeContext);
                    if(!exprType){
                        return false;
                    }
                    break;
                }
            }
        };
        return true;
    }

    bool SemanticA::checkSymbolsForStmt(ASTStmt *stmt, Semantics::STableContext &symbolTableContext){
        return checkSymbolsForStmtInScope(stmt,symbolTableContext,ASTScopeGlobal);
    }

    
}
